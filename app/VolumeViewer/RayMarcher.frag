#version 460

#define M_PI    3.14159265358979323846
#define M_1_PI  0.318309886183790671538
#define M_1_2PI 0.5 * M_1_PI

layout(location = 0) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;

layout(constant_id = 0) const int MAX_STEPS = 1000;
layout(constant_id = 1) const float STEP_SIZE = 0.002;
layout(constant_id = 2) const float DENSITY = 1.0;

// Feature flags, must match VolumeViewer.cpp
const uint FLAG_PATH_TRACING     = 1u << 0;
const uint FLAG_ACCUMULATE       = 1u << 1;
const uint FLAG_MULTI_SCATTER    = 1u << 2;
const uint FLAG_NEE              = 1u << 3;
const uint FLAG_RUSSIAN_ROULETTE = 1u << 4;
const uint FLAG_ENV_LIGHT        = 1u << 5;
const uint FLAG_HG_PHASE         = 1u << 6;
const uint FLAG_TONEMAP          = 1u << 7;
const uint FLAG_ENV_IMPORTANCE   = 1u << 8;
const uint FLAG_MIS              = 1u << 9;
const uint FLAG_DEBUG_TRUNCATION = 1u << 10;

// Value of the NEE depth field that means "no limit"
const int NEE_DEPTH_UNLIMITED = 255;

// Optical depth beyond which a shadow ray is considered fully blocked (exp(-24) is far below one 32-bit float ULP
// of the accumulated result). Without this, dense volumes make every shadow march run to MAX_STEPS.
const float TAU_CUTOFF = 24.0;

layout(set = 0, binding = 0) uniform CameraUniformDynamic {
    mat4 view;
    mat4 view_inv;
    mat4 proj;
    mat4 proj_inv;
} camera;

layout(set = 1, binding = 0) uniform sampler3D volume;
layout(set = 1, binding = 1) uniform sampler2D environmentMap;
layout(set = 1, binding = 2, rgba32f) uniform image2D accumImage;

// Cumulative distribution functions for importance sampling the environment map, normalized to [0, 1]. The marginal
// selects a row, the conditional selects a texel within that row.
layout(std430, set = 1, binding = 3) readonly buffer MarginalDistribution {
    float marginalCdf[];
};

layout(std430, set = 1, binding = 4) readonly buffer ConditionalDistribution {
    float conditionalCdf[];
};

// Push constants are limited to 128 bytes on some devices, hence the packing of bounces and samples
layout(push_constant) uniform PushConstant {
    mat4 model_inv;
    vec3 gridMin;
    float phaseG;
    vec3 gridMax;
    float envIntensity;
    vec2 viewPort;
    uint frameIndex;
    uint flags;
    uint bouncesAndSamples;
    float albedo;
    uint seed;
    float exposure;
} pc;

int maxBounces()
{
    return int(pc.bouncesAndSamples & 0xffffu);
}

int samplesPerFrame()
{
    return int((pc.bouncesAndSamples >> 16) & 0xffu);
}

int neeDepth()
{
    return int(pc.bouncesAndSamples >> 24);
}

// Beyond the NEE depth the shadow march is skipped and the environment is estimated by the escaping ray alone.
// That stays unbiased as long as those escapes are then counted in full rather than MIS weighted.
bool neeActiveAt(int scatterEvent)
{
    int depth = neeDepth();
    return depth >= NEE_DEPTH_UNLIMITED || scatterEvent < depth;
}

// --------------------------------------------------------------------------------------------------
// Random number generation (PCG)
// --------------------------------------------------------------------------------------------------

uint rngState;

uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void initRNG(uvec2 pixel, uint seed)
{
    rngState = pcgHash(pixel.x + 1973u * pixel.y + 9277u * seed);
}

float rand()
{
    rngState = rngState * 747796405u + 2891336453u;
    uint word = ((rngState >> ((rngState >> 28u) + 4u)) ^ rngState) * 277803737u;
    return float((word >> 22u) ^ word) * (1.0 / 4294967296.0);
}

vec2 rand2()
{
    return vec2(rand(), rand());
}

// --------------------------------------------------------------------------------------------------
// Environment
// --------------------------------------------------------------------------------------------------

vec2 worldToLatlongMap(vec3 dir)
{
    vec3 p = normalize(dir);
    vec2 uv;
    uv.x = atan(-p.z, p.x) * M_1_2PI + 0.5;
    uv.y = acos(-p.y) * M_1_PI;
    return uv;
}

vec3 latlongMapToWorld(float u, float v)
{
    float theta = v * M_PI;
    float phi = (u - 0.5) * 2.0 * M_PI;
    float sinTheta = sin(theta);
    return vec3(sinTheta * cos(phi), -cos(theta), -sinTheta * sin(phi));
}

vec3 environmentRadiance(vec3 dir)
{
    vec3 radiance = vec3(1.0);
    if ((pc.flags & FLAG_ENV_LIGHT) != 0u) {
        radiance = texture(environmentMap, worldToLatlongMap(dir)).rgb;
    }
    return pc.envIntensity * radiance;
}

// Largest index i in [0, n - 1] such that cdf[i] <= xi, where the CDF holds n + 1 values
int findIntervalMarginal(int n, float xi)
{
    int first = 0;
    int len = n + 1;
    while (len > 0) {
        int halfLen = len >> 1;
        int middle = first + halfLen;
        if (marginalCdf[middle] <= xi) {
            first = middle + 1;
            len -= halfLen + 1;
        } else {
            len = halfLen;
        }
    }
    return clamp(first - 1, 0, n - 1);
}

int findIntervalConditional(int base, int n, float xi)
{
    int first = 0;
    int len = n + 1;
    while (len > 0) {
        int halfLen = len >> 1;
        int middle = first + halfLen;
        if (conditionalCdf[base + middle] <= xi) {
            first = middle + 1;
            len -= halfLen + 1;
        } else {
            len = halfLen;
        }
    }
    return clamp(first - 1, 0, n - 1);
}

// Sample a direction proportionally to the radiance in the environment map, by inverting the marginal CDF over rows
// and then the conditional CDF within the chosen row. Returns the pdf with respect to solid angle.
vec3 sampleEnvironmentDirection(out float pdf)
{
    ivec2 size = textureSize(environmentMap, 0);

    float xiU = rand();
    float xiV = rand();

    int row = findIntervalMarginal(size.y, xiV);
    float marginalBin = marginalCdf[row + 1] - marginalCdf[row];
    float dv = marginalBin > 0.0 ? (xiV - marginalCdf[row]) / marginalBin : 0.0;
    float v = (float(row) + dv) / float(size.y);

    int base = row * (size.x + 1);
    int col = findIntervalConditional(base, size.x, xiU);
    float conditionalBin = conditionalCdf[base + col + 1] - conditionalCdf[base + col];
    float du = conditionalBin > 0.0 ? (xiU - conditionalCdf[base + col]) / conditionalBin : 0.0;
    float u = (float(col) + du) / float(size.x);

    float sinTheta = sin(v * M_PI);
    if (sinTheta <= 0.0) {
        pdf = 0.0;
        return vec3(0.0, 1.0, 0.0);
    }

    // Convert the density from texture space to solid angle, where dw = 2 pi^2 sin(theta) du dv
    float pdfTexture = (marginalBin * float(size.y)) * (conditionalBin * float(size.x));
    pdf = pdfTexture / (2.0 * M_PI * M_PI * sinTheta);

    return latlongMapToWorld(u, v);
}

// The pdf that sampleEnvironmentDirection would have produced for a given direction, needed to weight the two
// sampling strategies against each other. Falls back to the uniform pdf when the distribution is not in use.
float environmentPdf(vec3 dir)
{
    const uint importanceSample = FLAG_ENV_IMPORTANCE | FLAG_ENV_LIGHT;
    if ((pc.flags & importanceSample) != importanceSample) {
        return 1.0 / (4.0 * M_PI);
    }

    ivec2 size = textureSize(environmentMap, 0);
    vec2 uv = worldToLatlongMap(dir);

    float sinTheta = sin(uv.y * M_PI);
    if (sinTheta <= 0.0) {
        return 0.0;
    }

    int col = clamp(int(uv.x * float(size.x)), 0, size.x - 1);
    int row = clamp(int(uv.y * float(size.y)), 0, size.y - 1);
    int base = row * (size.x + 1);

    float marginalBin = marginalCdf[row + 1] - marginalCdf[row];
    float conditionalBin = conditionalCdf[base + col + 1] - conditionalCdf[base + col];

    return (marginalBin * float(size.y)) * (conditionalBin * float(size.x)) / (2.0 * M_PI * M_PI * sinTheta);
}

// Power heuristic with an exponent of two
float misWeight(float pdfA, float pdfB)
{
    float a = pdfA * pdfA;
    float b = pdfB * pdfB;
    float denom = a + b;
    return denom > 0.0 ? a / denom : 0.0;
}

// --------------------------------------------------------------------------------------------------
// Volume
// --------------------------------------------------------------------------------------------------

vec3 toModel(vec3 p)
{
    return (pc.model_inv * vec4(p, 1.0)).xyz;
}

vec3 toModelDir(vec3 d)
{
    return (pc.model_inv * vec4(d, 0.0)).xyz;
}

float densityAt(vec3 posModel)
{
    vec3 uvw = (posModel - pc.gridMin) / (pc.gridMax - pc.gridMin);
    return DENSITY * texture(volume, uvw).r;
}

// Intersect a ray with the volume AABB. Origin and direction are in model space, but the returned t
// range is shared with the world-space ray since the model transform is affine.
bool intersectVolume(vec3 o, vec3 d, out float tEnter, out float tExit)
{
    vec3 invD = 1.0 / d;
    vec3 tLo = (pc.gridMin - o) * invD;
    vec3 tHi = (pc.gridMax - o) * invD;
    vec3 tMin = min(tLo, tHi);
    vec3 tMax = max(tLo, tHi);
    tEnter = max(max(tMin.x, tMin.y), max(tMin.z, 0.0));
    tExit = min(min(tMax.x, tMax.y), tMax.z);
    return tExit > tEnter;
}

// Sample a free-flight distance by marching until the accumulated optical depth reaches -log(1 - xi). Returns the
// scattering distance, or a negative value if the ray did not scatter. The density sampled at t is taken to hold
// over the step that starts there, so the sample covers [t, t + STEP_SIZE].
//
// exhausted is set when the march ran out of steps while still inside the volume. The ray neither scattered nor
// left, so the caller must not treat it as having reached the environment.
float sampleScatterDistance(vec3 o, vec3 d, float tEnter, float tExit, out bool exhausted)
{
    exhausted = false;

    float tauTarget = -log(max(1.0 - rand(), 1e-7));
    float tau = 0.0;
    float t = tEnter + rand() * STEP_SIZE;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (t >= tExit) {
            return -1.0;
        }

        float sigma = densityAt(o + t * d);
        tau += sigma * STEP_SIZE;
        if (tau >= tauTarget) {
            // The target was passed somewhere inside the step that ends at t + STEP_SIZE, so walk back from its end
            float overshoot = (tau - tauTarget) / max(sigma, 1e-6);
            return t + STEP_SIZE - min(overshoot, STEP_SIZE);
        }
        t += STEP_SIZE;
    }

    exhausted = true;
    return -1.0;
}

// Transmittance along a shadow ray. Returns zero if the march runs out of steps before leaving the volume, since
// the part that was never marched could contain any amount of medium and assuming it is empty would leak light.
float transmittance(vec3 o, vec3 d, float tEnter, float tExit)
{
    float tau = 0.0;
    float t = tEnter + 0.5 * STEP_SIZE;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (t >= tExit) {
            return exp(-tau);
        }

        tau += densityAt(o + t * d) * STEP_SIZE;
        if (tau > TAU_CUTOFF) {
            return 0.0;
        }
        t += STEP_SIZE;
    }

    return 0.0;
}

// --------------------------------------------------------------------------------------------------
// Phase functions
// --------------------------------------------------------------------------------------------------

float phaseEval(float cosTheta)
{
    if ((pc.flags & FLAG_HG_PHASE) != 0u) {
        float g = pc.phaseG;
        // The floor has to apply to the whole denominator, not just the square root, or this stops being exactly
        // the density that sampleHGCos() draws from and the MIS weights no longer match the sampling
        float denom = max(1.0 + g * g - 2.0 * g * cosTheta, 1e-4);
        return (1.0 - g * g) / (4.0 * M_PI * denom * sqrt(denom));
    }
    return 1.0 / (4.0 * M_PI);
}

float sampleHGCos(float g, float xi)
{
    if (abs(g) < 1e-3) {
        return 1.0 - 2.0 * xi;
    }
    float sq = (1.0 - g * g) / (1.0 + g - 2.0 * g * xi);
    return (1.0 + g * g - sq * sq) / (2.0 * g);
}

void onb(vec3 n, out vec3 t, out vec3 b)
{
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float m = n.x * n.y * a;
    t = vec3(1.0 + s * n.x * n.x * a, s * m, -s * n.x);
    b = vec3(m, s + n.y * n.y * a, -n.y);
}

vec3 sampleSphere()
{
    float z = 1.0 - 2.0 * rand();
    float phi = 2.0 * M_PI * rand();
    float r = sqrt(max(0.0, 1.0 - z * z));
    return vec3(r * cos(phi), r * sin(phi), z);
}

vec3 samplePhaseDirection(vec3 dir)
{
    float cosTheta;
    if ((pc.flags & FLAG_HG_PHASE) != 0u) {
        cosTheta = sampleHGCos(pc.phaseG, rand());
    } else {
        cosTheta = 1.0 - 2.0 * rand();
    }
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = 2.0 * M_PI * rand();
    vec3 t, b;
    onb(dir, t, b);
    return normalize(sinTheta * cos(phi) * t + sinTheta * sin(phi) * b + cosTheta * dir);
}

// --------------------------------------------------------------------------------------------------
// Path tracing
// --------------------------------------------------------------------------------------------------

// Traces one path. Sets truncated if the path was still alive when it ran into the bounce limit, which is the one
// place where energy is dropped rather than accounted for.
vec3 tracePath(vec3 origin, vec3 dir, out bool truncated)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);

    truncated = false;

    int bounceLimit = (pc.flags & FLAG_MULTI_SCATTER) != 0u ? maxBounces() : 1;

    bool nee = (pc.flags & FLAG_NEE) != 0u;
    bool mis = nee && (pc.flags & FLAG_MIS) != 0u;

    // Pdf of the phase-sampled direction the current ray is travelling in, needed to weight the environment
    // radiance against next event estimation if the ray escapes
    float phasePdf = 0.0;

    for (int bounce = 0; bounce <= bounceLimit; bounce++) {
        // Weight for the environment radiance should this ray escape. The camera ray is not something next
        // event estimation could have sampled, so it always counts in full. After a scattering event the
        // same radiance is also estimated by next event estimation: with MIS the two strategies are combined,
        // without it the escaped ray is dropped and next event estimation accounts for all of it. Past the NEE
        // depth no shadow ray was traced, so the escape carries the whole contribution again.
        float escapeWeight = 1.0;
        if (bounce > 0 && nee && neeActiveAt(bounce - 1)) {
            escapeWeight = mis ? misWeight(phasePdf, environmentPdf(dir)) : 0.0;
        }

        vec3 o = toModel(origin);
        vec3 d = toModelDir(dir);

        float tEnter, tExit;
        if (!intersectVolume(o, d, tEnter, tExit)) {
            if (escapeWeight > 0.0) {
                radiance += escapeWeight * throughput * environmentRadiance(dir);
            }
            break;
        }

        bool exhausted;
        float tScatter = sampleScatterDistance(o, d, tEnter, tExit, exhausted);
        if (tScatter < 0.0) {
            // A ray that ran out of steps is still inside the volume, so it has not reached the environment and
            // must not collect any of it
            if (escapeWeight > 0.0 && !exhausted) {
                radiance += escapeWeight * throughput * environmentRadiance(dir);
            }
            truncated = truncated || exhausted;
            break;
        }

        // The path is still carrying energy but has run out of bounces, so this is where the estimator loses
        // energy. Russian roulette terminating a path is not truncation, it is compensated for.
        if (bounce == bounceLimit) {
            truncated = true;
            break;
        }

        // Scattering event
        origin += tScatter * dir;
        throughput *= pc.albedo;

        // Next event estimation: sample a direction towards the environment and estimate the
        // transmittance with a shadow march
        if (nee && neeActiveAt(bounce)) {
            vec3 lightDir;
            float pdf;

            // Importance sampling the map is only meaningful when the map is what is lighting the scene
            const uint importanceSample = FLAG_ENV_IMPORTANCE | FLAG_ENV_LIGHT;
            if ((pc.flags & importanceSample) == importanceSample) {
                lightDir = sampleEnvironmentDirection(pdf);
            } else {
                lightDir = sampleSphere();
                pdf = 1.0 / (4.0 * M_PI);
            }

            if (pdf > 0.0) {
                // Sampling the phase function is exact, so the phase value doubles as the pdf that the other
                // strategy would have had for this direction
                float phaseValue = phaseEval(dot(dir, lightDir));
                float weight = mis ? misWeight(pdf, phaseValue) : 1.0;

                if (weight > 0.0) {
                    vec3 lo = toModel(origin);
                    vec3 ld = toModelDir(lightDir);
                    float sEnter, sExit;
                    float visibility = 1.0;
                    if (intersectVolume(lo, ld, sEnter, sExit)) {
                        visibility = transmittance(lo, ld, sEnter, sExit);
                    }
                    radiance +=
                        weight * throughput * phaseValue * visibility * environmentRadiance(lightDir) / pdf;
                }
            }
        }

        vec3 scatterDir = samplePhaseDirection(dir);
        phasePdf = phaseEval(dot(dir, scatterDir));
        dir = scatterDir;

        // Russian roulette after a few bounces. The survival probability must be allowed to reach 1.0: clamping it
        // lower would kill paths in a high-albedo medium such as a cloud, where hundreds of scattering events are
        // needed before a photon escapes.
        if ((pc.flags & FLAG_RUSSIAN_ROULETTE) != 0u && bounce >= 3) {
            float p = clamp(max(throughput.r, max(throughput.g, throughput.b)), 0.05, 1.0);
            if (rand() >= p) {
                break;
            }
            throughput /= p;
        }
    }

    return radiance;
}

vec3 cameraRay(vec2 jitter, out vec3 origin)
{
    const vec2 uv = (gl_FragCoord.xy + jitter) / pc.viewPort;
    vec2 d = uv * 2.0 - 1.0;

    origin = (camera.view_inv * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec4 target = camera.proj_inv * vec4(d.x, d.y, 1.0, 1.0);
    return normalize((camera.view_inv * vec4(normalize(target.xyz), 0.0)).xyz);
}

// --------------------------------------------------------------------------------------------------
// Display
// --------------------------------------------------------------------------------------------------

vec3 tonemapACES(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// The swapchain is a UNORM format, so the sRGB transfer function has to be applied here
vec3 linearToSrgb(vec3 c)
{
    return mix(12.92 * c, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, greaterThan(c, vec3(0.0031308)));
}

vec3 display(vec3 radiance)
{
    radiance *= pc.exposure;
    if ((pc.flags & FLAG_TONEMAP) == 0u) {
        return clamp(radiance, 0.0, 1.0);
    }
    return linearToSrgb(tonemapACES(radiance));
}

// Original single-pass alpha compositing, kept for comparison
vec4 simpleMarch(vec3 origin, vec3 dir)
{
    vec4 color = vec4(0.0);
    float t = 0.0;
    for (int i = 0; i < MAX_STEPS && color.a < 1.0; i++) {
        float density = densityAt(toModel(origin + t * dir));
        float alpha = 1.0 - exp(-density * STEP_SIZE);

        vec3 sampleColor = vec3(1.0);
        sampleColor *= alpha;

        color.rgb += (1.0 - color.a) * sampleColor;
        color.a += (1.0 - color.a) * alpha;

        t += STEP_SIZE;
    }
    return color;
}

void main()
{
    vec3 origin;

    if ((pc.flags & FLAG_PATH_TRACING) == 0u) {
        vec3 dir = cameraRay(vec2(0.0), origin);
        fragColor = simpleMarch(origin, dir);
        return;
    }

    initRNG(uvec2(gl_FragCoord.xy), pc.seed);

    bool accumulate = (pc.flags & FLAG_ACCUMULATE) != 0u;

    vec3 total = vec3(0.0);
    int truncatedCount = 0;
    int spp = max(samplesPerFrame(), 1);
    for (int s = 0; s < spp; s++) {
        vec2 jitter = accumulate ? rand2() - 0.5 : vec2(0.0);
        vec3 dir = cameraRay(jitter, origin);
        bool truncated;
        total += tracePath(origin, dir, truncated);
        if (truncated) {
            truncatedCount++;
        }
    }
    vec3 radiance = total / float(spp);

    // A single non-finite sample would otherwise poison the pixel for the rest of the accumulation
    if (any(isnan(radiance)) || any(isinf(radiance))) {
        radiance = vec3(0.0);
    }

    // Accumulate the truncation rate in place of radiance so that it converges the same way
    bool debugTruncation = (pc.flags & FLAG_DEBUG_TRUNCATION) != 0u;
    if (debugTruncation) {
        radiance = vec3(float(truncatedCount) / float(spp));
    }

    if (accumulate) {
        ivec2 coord = ivec2(gl_FragCoord.xy);
        // Alpha channel holds the accumulated frame count
        vec4 accum = vec4(radiance, 1.0);
        if (pc.frameIndex != 0u) {
            accum += imageLoad(accumImage, coord);
        }
        imageStore(accumImage, coord, accum);
        radiance = accum.rgb / accum.a;
    }

    // The truncation rate is a fraction, not a radiance, so it is shown as is
    fragColor = vec4(debugTruncation ? radiance : display(radiance), 1.0);
}
