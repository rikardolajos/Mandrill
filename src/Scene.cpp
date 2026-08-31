#include "Scene.h"

#include "Extension.h"
#include "Helpers.h"
#include "Log.h"
#include "Pipeline.h"
#include "Shader.h"

#include "tiny_obj_loader.h"
#include "tinygltf/tiny_gltf.h"

using namespace Mandrill;

Node::Node()
{
    mTransform = glm::identity<glm::mat4>();
    mVisible = true;
    mTransformIndex = 0;
}

Node::~Node()
{
}

void Node::drawMeshes(VkCommandBuffer cmd, const ptr<const Scene> pScene) const
{
    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];

        // Bind vertex and index buffers
        std::array<VkBuffer, 1> vertexBuffers = {pScene->mpVertexBuffer->getBuffer()};
        std::array<VkDeviceSize, 1> offsets = {mesh.deviceVerticesOffset};
        vkCmdBindVertexBuffers(cmd, 0, count(vertexBuffers), vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(cmd, pScene->mpIndexBuffer->getBuffer(), mesh.deviceIndicesOffset, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, count(mesh.indices), 1, 0, 0, 0);
    }
}

void Node::render(VkCommandBuffer cmd, const ptr<const Scene> pScene, uint32_t frameInFlightIndex) const
{
    if (!mVisible || !mpPipeline) {
        return;
    }

    frameInFlightIndex = pScene->mpDevice->resolveFrameInFlightIndex(frameInFlightIndex);

    mpPipeline->bind(cmd);

    auto pShader = mpPipeline->getShader();

    // The scene prepares one of these per shader its nodes are rendered with. A node that was moved to a pipeline
    // whose shader the scene never saw has nothing to bind.
    const Scene::ShaderResources* pResources = pScene->findShaderResources(pShader.get());
    if (!pResources) {
        Log::Error("Node::render() - The scene has no resources attached to this node's shader. Set the pipelines of "
                   "all nodes before calling Scene::createDescriptors().");
        return;
    }

    pScene->mpTransforms->copyFromHost(&mTransform, mTransformIndex + frameInFlightIndex);

    // The camera and the environment map are bound here rather than once for the whole scene, since every node can
    // carry its own pipeline and the resources belong to that pipeline's shader

    // Camera matrices, which the shader selects this frame's copy of on its own
    auto cameraInfo = pShader->getResourceInfo("camera");
    if (cameraInfo) {
        pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cameraInfo->set, frameInFlightIndex);
    }

    // The whole scene shares one transforms buffer, so the offset has to select both this node's slot and the copy
    // belonging to this frame in flight
    auto transformInfo = pShader->getResourceInfo("mesh");
    if (transformInfo) {
        uint32_t transformOffset = pScene->mpTransforms->getOffset(mTransformIndex + frameInFlightIndex);
        pShader->bindResourcesWithOffsets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, transformInfo->set, {transformOffset});
    }

    if (pScene->mpEnvironmentMap) {
        auto environmentInfo = pShader->getResourceInfo("environmentMap");
        if (environmentInfo) {
            pShader->bindResources(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, environmentInfo->set);
        }
    }

    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];

        // Materials keep a prepared set each, so switching material is a single bind
        pResources->materialDescriptors[mesh.materialIndex]->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                                  mpPipeline->getLayout(), pResources->materialSet);

        // Bind vertex and index buffers
        std::array<VkBuffer, 1> vertexBuffers = {pScene->mpVertexBuffer->getBuffer()};
        std::array<VkDeviceSize, 1> offsets = {mesh.deviceVerticesOffset};
        vkCmdBindVertexBuffers(cmd, 0, count(vertexBuffers), vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(cmd, pScene->mpIndexBuffer->getBuffer(), mesh.deviceIndicesOffset, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, count(mesh.indices), 1, 0, 0, 0);
    }
}

AABB Node::getBoundingBox(const ptr<const Scene> pScene) const
{
    AABB boundingBox = {};
    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];
        boundingBox.expand(mesh.boundingBox);
    }
    return boundingBox;
}

Scene::Scene(ptr<Device> pDevice) : mpDevice(pDevice), mVertexCount(0), mIndexCount(0)
{
    const uint8_t data[] = {0xff, 0x00, 0xff, 0xff, 0x88, 0x00, 0xff, 0xff,
                            0x88, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff};
    const uint32_t width = 2;
    const uint32_t height = 2;
    const uint32_t depth = 1;
    const uint32_t bytesPerPixel = 4;
    mpMissingTexture = pDevice->createTextureFromBuffer(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, data, width,
                                                        height, depth, bytesPerPixel);
    mTextures.insert(std::make_pair("", mpMissingTexture));

    // Default material. glTF defaults a material to fully metallic, which is a poor stand-in for one that was never
    // authored at all, so make the fallback a rough dielectric instead.
    Material defaultMaterial;
    defaultMaterial.params.metallic = 0.0f;
    mMaterials.push_back(defaultMaterial);
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, bool frustumCulling,
                   uint32_t frameInFlightIndex) const
{
    if (mNodes.empty()) {
        return;
    }

    frameInFlightIndex = mpDevice->resolveFrameInFlightIndex(frameInFlightIndex);

    Frustum cameraFrustum = pCamera->getFrustum(frameInFlightIndex);
    for (auto& node : mNodes) {
        // Cull nodes that are outside of the camera's frustum
        if (frustumCulling) {
            AABB nodeBoundingBox = node.getBoundingBox(shared_from_this());
            nodeBoundingBox.transform(node.getTransform()); // Transform bounding box to world space
            if (!cameraFrustum.intersects(nodeBoundingBox)) {
                continue;
            }
        }

        node.render(cmd, shared_from_this(), frameInFlightIndex);
    }
}

uint32_t Scene::addNode()
{
    Node node = {};

    mNodes.push_back(node);

    return count(mNodes) - 1;
}

static glm::mat4 extractTransform(const tinygltf::Node& node)
{
    glm::mat4 transform = glm::identity<glm::mat4>();
    glm::mat4 T = glm::identity<glm::mat4>();
    glm::mat4 R = glm::identity<glm::mat4>();
    glm::mat4 S = glm::identity<glm::mat4>();
    if (node.matrix.size() == 16) {
        transform = glm::make_mat4(node.matrix.data());
    } else {
        if (node.translation.size() == 3) {
            T = glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(node.translation[0]),
                                                          static_cast<float>(node.translation[1]),
                                                          static_cast<float>(node.translation[2])));
        }
        if (node.rotation.size() == 4) {
            glm::quat rotationQuat =
                glm::quat(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                          static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
            R = glm::mat4_cast(rotationQuat);
        }
        if (node.scale.size() == 3) {
            S = glm::scale(glm::mat4(1.0f),
                           glm::vec3(static_cast<float>(node.scale[0]), static_cast<float>(node.scale[1]),
                                     static_cast<float>(node.scale[2])));
        }
        transform = T * R * S;
    }
    return transform;
}

std::vector<uint32_t> Scene::addNodesFromFile(const std::filesystem::path& path,
                                              const std::filesystem::path& materialPath)
{
    std::vector<uint32_t> newNodeIndices;
    auto newMeshIndices = addMeshFromFile(path, materialPath);

    if (path.extension() == ".obj") {
        // Create a node for each mesh
        for (auto meshIndex : newMeshIndices) {
            auto nodeIndex = addNode();
            mNodes[nodeIndex].addMesh(meshIndex);
            newNodeIndices.push_back(nodeIndex);
        }
    } else if (path.extension() == ".gltf" || path.extension() == ".glb") {
        // glTF contains nodes, so we need to extract the transforms as well
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err;
        std::string warn;

        bool ret = false;
        if (path.extension() == ".gltf") {
            ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
        } else if (path.extension() == ".glb") {
            ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
        }

        if (!ret) {
            // addMeshFromFile() above has already reported the details
            return newNodeIndices;
        }

        // A glTF node refers to a glTF mesh, but addMeshFromFile() created one Mandrill mesh per primitive, appended
        // to any meshes the scene already held. Walking the primitives in the same order recovers which Mandrill
        // meshes belong to which glTF mesh.
        std::vector<std::vector<uint32_t>> gltfMeshToMeshIndices(model.meshes.size());
        size_t nextMeshIndex = 0;
        for (size_t m = 0; m < model.meshes.size(); m++) {
            for (size_t p = 0; p < model.meshes[m].primitives.size() && nextMeshIndex < newMeshIndices.size(); p++) {
                gltfMeshToMeshIndices[m].push_back(newMeshIndices[nextMeshIndex++]);
            }
        }

        // Load scenes if available

        struct ParseNode {
            int index;
            glm::mat4 parentTransform = glm::identity<glm::mat4>();
        };

        std::stack<ParseNode> parseNodeStack;
        if (!model.scenes.empty()) {
            // Push all nodes from scenes
            for (const auto& scene : model.scenes) {
                for (auto nodeIndex : scene.nodes) {
                    parseNodeStack.push({nodeIndex, glm::identity<glm::mat4>()});
                }
            }
        } else {
            // Otherwise load all nodes
            for (size_t i = 0; i < model.nodes.size(); i++) {
                parseNodeStack.push({static_cast<int>(i), glm::identity<glm::mat4>()});
            }
        }

        while (!parseNodeStack.empty()) {
            ParseNode parseNode = parseNodeStack.top();
            parseNodeStack.pop();

            const tinygltf::Node& node = model.nodes[parseNode.index];
            glm::mat4 transform = parseNode.parentTransform * extractTransform(node);

            // Process node
            if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < gltfMeshToMeshIndices.size()) {
                auto mandrillNodeIndex = addNode();
                for (auto meshIndex : gltfMeshToMeshIndices[node.mesh]) {
                    mNodes[mandrillNodeIndex].addMesh(meshIndex);
                }
                mNodes[mandrillNodeIndex].setTransform(transform);
                newNodeIndices.push_back(mandrillNodeIndex);
            }

            // Push child nodes to stack
            for (auto childIndex : node.children) {
                parseNodeStack.push({childIndex, transform});
            }
        }
    }

    return newNodeIndices;
}

uint32_t Scene::addMaterial(Material material)
{
    auto setTexture = [this, &material](const std::string& texturePath, MaterialTextureBit bit) {
        if (texturePath.empty()) {
            mTextures.insert(std::make_pair(texturePath, mpMissingTexture));
            return;
        }
        addTexture(texturePath);
        material.params.hasTexture |= static_cast<uint32_t>(bit);
    };

    material.params.hasTexture = 0;

    setTexture(material.baseColorTexturePath, MaterialTextureBit::BaseColor);
    setTexture(material.metallicRoughnessTexturePath, MaterialTextureBit::MetallicRoughness);
    setTexture(material.occlusionTexturePath, MaterialTextureBit::Occlusion);
    setTexture(material.emissiveTexturePath, MaterialTextureBit::Emissive);
    setTexture(material.normalTexturePath, MaterialTextureBit::Normal);

    mMaterials.push_back(material);

    return count(mMaterials) - 1;
}

uint32_t Scene::addMesh(const std::vector<Vertex> vertices, const std::vector<uint32_t> indices, uint32_t materialIndex)
{
    std::vector<glm::vec3> positions;
    for (const auto& vertex : vertices) {
        positions.push_back(vertex.position);
    }
    Mesh mesh = {
        .vertices = vertices,
        .indices = indices,
        .materialIndex = materialIndex,
        .boundingBox = AABB::calculate(positions),
    };

    mMeshes.push_back(mesh);

    return count(mMeshes) - 1;
}

template <typename T, typename... Rest> inline void hashCombine(std::size_t& seed, T const& v, Rest&&... rest)
{
    std::hash<T> hasher;
    seed ^= (hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    int i[] = {0, (hashCombine(seed, std::forward<Rest>(rest)), 0)...};
    (void)(i);
}

namespace std
{
    template <> struct hash<Vertex> {
        /// <summary>
        /// Genereate a hash for a vertex.
        /// </summary>
        /// <param name="vertex">Vertex used as hash input</param>
        /// <returns>Hash value</returns>
        size_t operator()(Vertex const& vertex) const
        {
            size_t h = 0;
            hashCombine(h, vertex.position, vertex.normal, vertex.texcoord, vertex.tangent, vertex.binormal);
            return h;
        }
    };
} // namespace std

std::vector<uint32_t> Scene::addMeshFromFile(const std::filesystem::path& path,
                                             const std::filesystem::path& materialPath)
{
    std::vector<uint32_t> newMeshIndices;

    Log::Info("Loading {}", path.string());

    if (path.extension() == ".obj") {
        newMeshIndices = loadFromOBJ(path, materialPath);
    } else if (path.extension() == ".gltf" || path.extension() == ".glb") {
        newMeshIndices = loadFromGLTF(path);
    } else {
        Log::Error("Unsupported file format: {}", path.extension().string());
        return {};
    }

    // Add to statistics
    for (auto index : newMeshIndices) {
        mVertexCount += count(mMeshes[index].vertices);
        mIndexCount += count(mMeshes[index].indices);
    }

    return newMeshIndices;
}

void Scene::compile()
{
    const uint32_t framesInFlightCount = mpDevice->getFramesInFlightCount();

    if (mpMissingTexture->getSampler() == VK_NULL_HANDLE) {
        Log::Error("Scene: Sampler must be set before calling compile()");
    }

    // Calculate size of buffers
    size_t verticesSize = 0;
    size_t indicesSize = 0;
    for (auto& node : mNodes) {
        for (auto meshIndex : node.mMeshIndices) {
            auto& mesh = mMeshes[meshIndex];
            verticesSize += sizeof(Vertex) * mesh.vertices.size();
            indicesSize += sizeof(uint32_t) * mesh.indices.size();
        }
    }

    // Allocate device buffers
    mpVertexBuffer =
        mpDevice->createBuffer(verticesSize,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpIndexBuffer =
        mpDevice->createBuffer(indicesSize,
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;

    // Material parameters are bound with a descriptor offset, so every entry has to start on an aligned boundary.
    // That alignment, not the size of the struct, is the stride to use when addressing the entries from the host too.
    VkDeviceSize materialParamsStride = Helpers::alignTo(sizeof(MaterialParams), alignment);

    // Transforms can change between frames, material parameters can not
    mpTransforms = mpDevice->createDynamicBuffer(sizeof(glm::mat4), count(mNodes) * framesInFlightCount);

    VkDeviceSize materialParamsSize = materialParamsStride * mMaterials.size();
    mpMaterialParams = mpDevice->createBuffer(materialParamsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Associate each node with a part of the transforms buffer, with one copy for each frame in flight
    const glm::mat4 identity = glm::identity<glm::mat4>();
    for (uint32_t i = 0; i < count(mNodes); i++) {
        mNodes[i].mTransformIndex = i * framesInFlightCount;
        for (uint32_t c = 0; c < framesInFlightCount; c++) {
            mpTransforms->copyFromHost(&identity, mNodes[i].mTransformIndex + c);
        }
    }

    // Associate each material with a part of the material params buffer
    std::byte* materialParams = static_cast<std::byte*>(mpMaterialParams->getHostMap());
    for (uint32_t i = 0; i < count(mMaterials); i++) {
        mMaterials[i].paramsOffset = i * materialParamsStride;
        mMaterials[i].paramsDevice = reinterpret_cast<MaterialParams*>(materialParams + mMaterials[i].paramsOffset);
        *mMaterials[i].paramsDevice = mMaterials[i].params;
    }

    // For ray tracing a global list is used and this struct keeps track of the texture indices
    VkDeviceSize materialBufferSize = sizeof(MaterialDevice) * mMaterials.size();
    mpMaterialBuffer = mpDevice->createBuffer(materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    MaterialDevice* materials = static_cast<MaterialDevice*>(mpMaterialBuffer->getHostMap());
    for (uint32_t i = 0; i < count(mMaterials); i++) {
        materials[i].params = mMaterials[i].params;
        auto textureIndex = [this](const std::string& texturePath) {
            return static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(texturePath)));
        };
        materials[i].baseColorTextureIndex = textureIndex(mMaterials[i].baseColorTexturePath);
        materials[i].metallicRoughnessTextureIndex = textureIndex(mMaterials[i].metallicRoughnessTexturePath);
        materials[i].occlusionTextureIndex = textureIndex(mMaterials[i].occlusionTexturePath);
        materials[i].emissiveTextureIndex = textureIndex(mMaterials[i].emissiveTexturePath);
        materials[i].normalTextureIndex = textureIndex(mMaterials[i].normalTexturePath);
    }

    VkDeviceSize instanceDataBufferSize = sizeof(InstanceData) * mMeshes.size();
    mpInstanceDataBuffer = mpDevice->createBuffer(instanceDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    InstanceData* instanceData = static_cast<InstanceData*>(mpInstanceDataBuffer->getHostMap());
    uint32_t instanceIndex = 0;
    uint32_t verticesOffset = 0;
    uint32_t indicesOffset = 0;
    for (auto& node : mNodes) {
        for (auto& meshIndex : node.getMeshIndices()) {
            auto& mesh = mMeshes[meshIndex];

            instanceData[instanceIndex].verticesOffset = verticesOffset;
            instanceData[instanceIndex].indicesOffset = indicesOffset;

            verticesOffset += count(mesh.vertices);
            indicesOffset += count(mesh.indices);

            instanceIndex += 1;
        }
    }
}

void Scene::createDescriptors(ptr<Camera> pCamera)
{
    mShaderResources.clear();

    if (mNodes.empty()) {
        return; // Nothing is rendered, so there is nothing to attach the resources to
    }

    // The shaders to set up are the ones the nodes are rendered with, which is why the pipelines have to be in place
    // before this is called
    for (const auto& node : mNodes) {
        if (!node.mpPipeline) {
            continue;
        }

        ptr<Shader> pShader = node.mpPipeline->getShader();
        if (findShaderResources(pShader.get())) {
            continue; // Several nodes sharing a shader only need it set up once
        }

        createDescriptorsForShader(pShader, pCamera);
    }

    if (mShaderResources.empty()) {
        Log::Error("Scene::createDescriptors() - No node has a pipeline, so the scene cannot tell which shaders it is "
                   "rendered with. Call Node::setPipeline() before creating the descriptors.");
    }
}

const Scene::ShaderResources* Scene::findShaderResources(const Shader* pShader) const
{
    for (const auto& resources : mShaderResources) {
        if (resources.pShader.get() == pShader) {
            return &resources;
        }
    }
    return nullptr;
}

void Scene::createDescriptorsForShader(ptr<Shader> pShader, ptr<Camera> pCamera)
{
    // The camera matrices and the node transforms are single buffers that are rebound with an offset, so they are
    // attached once here. Which set and binding they land in comes from the shader.
    pShader->setResource("camera", pCamera->getUniformBuffer());
    pShader->setResource("mesh", mpTransforms->getBuffer(), 0, mpTransforms->getElementSize());

    // Both are dynamic, and they are bound with offsets that have nothing to do with each other, so a set holding
    // them both could only ever be bound for one of them
    auto cameraInfo = pShader->getResourceInfo("camera");
    auto transformInfo = pShader->getResourceInfo("mesh");
    if (cameraInfo && transformInfo && cameraInfo->set == transformInfo->set) {
        Log::Error("The camera and the node transforms are both in set {}, but they have to be in separate sets since "
                   "they are bound with different offsets.",
                   cameraInfo->set);
    }

    if (mpEnvironmentMap && pShader->hasResource("environmentMap")) {
        pShader->setResource("environmentMap", mpEnvironmentMap);
    }

    // A whole material is bound for every mesh, so the materials keep prepared sets instead of going through the
    // shader, which only holds one set per set index. Any material binding identifies the set they share.
    auto materialInfo = pShader->getResourceInfo("baseColorTexture");
    if (!materialInfo) {
        Log::Error("Shader has no baseColorTexture, so the scene cannot find which set the materials belong to");
        return;
    }

    ShaderResources resources;
    resources.pShader = pShader;
    resources.materialSet = materialInfo->set;
    resources.materialDescriptors.reserve(mMaterials.size());

    for (auto& mat : mMaterials) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mpMaterialParams, mat.paramsOffset,
                          sizeof(MaterialParams));
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.baseColorTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.metallicRoughnessTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.occlusionTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.emissiveTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.normalTexturePath]);

        // The descriptor writes the bindings in the order they are described, so the shader has to declare the
        // material resources in that same order within their set
        resources.materialDescriptors.push_back(
            mpDevice->createDescriptor(desc, pShader->getDescriptorSetLayout(resources.materialSet)));
    }

    mShaderResources.push_back(std::move(resources));
}

void Scene::createRayTracingDescriptors(ptr<Shader> pShader, ptr<Camera> pCamera,
                                        const ptr<AccelerationStructure> pAccelerationStructure)
{
    // Get a list of the textures
    std::vector<ptr<Texture>> textures;
    std::transform(mTextures.begin(), mTextures.end(), std::back_inserter(textures),
                   [](const auto& entry) { return entry.second; });

    pShader->setResource("camera", pCamera->getUniformBuffer());
    pShader->setResource("scene", pAccelerationStructure);
    pShader->setResource("vertexBuffer", mpVertexBuffer);
    pShader->setResource("indexBuffer", mpIndexBuffer);
    pShader->setResource("instanceDataBuffer", mpInstanceDataBuffer);
    pShader->setResource("materialBuffer", mpMaterialBuffer);
    pShader->setResource("textures", textures);

    if (mpEnvironmentMap && pShader->hasResource("environmentMap")) {
        pShader->setResource("environmentMap", mpEnvironmentMap);
    }
}

void Scene::syncToDevice()
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkDeviceSize verticesOffset = 0;
    VkDeviceSize indicesOffset = 0;

    for (auto& node : mNodes) {
        for (auto meshIndex : node.mMeshIndices) {
            auto& mesh = mMeshes[meshIndex];

            size_t vertSize = mesh.vertices.size() * sizeof(Vertex);
            size_t indxSize = mesh.indices.size() * sizeof(uint32_t);

            vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());

            mesh.deviceVerticesOffset = verticesOffset;
            mesh.deviceIndicesOffset = indicesOffset;

            verticesOffset += vertSize;
            indicesOffset += indxSize;
        }
    }

    mpVertexBuffer->copyFromHost(vertices.data(), verticesOffset, 0);
    mpIndexBuffer->copyFromHost(indices.data(), indicesOffset, 0);
}

static float perceivedBrightness(const glm::vec3& color)
{
    return std::sqrt(0.299f * color.r * color.r + 0.587f * color.g * color.g + 0.114f * color.b * color.b);
}

std::vector<uint32_t> Scene::loadFromOBJ(const std::filesystem::path& path, const std::filesystem::path& materialPath)
{
    std::vector<uint32_t> newMeshIndices;

    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = materialPath.string();
    readerConfig.triangulate = true;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path.string(), readerConfig)) {
        if (!reader.Error().empty()) {
            Log::Error("TinyObjReader: {}", reader.Error());
        }
        Log::Error("Failed to load {}", path.string());
    }

    if (!reader.Warning().empty()) {
        Log::Warning("TinyObjReader: {}", reader.Warning());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (auto& shape : shapes) {
        // One mesh per material in shape. Resolving the slot per vertex through std::distance on a set iterator is a
        // linear walk, so build the mapping once up front instead.
        std::set<int> matIDs(shape.mesh.material_ids.begin(), shape.mesh.material_ids.end());
        std::unordered_map<int, uint32_t> matIDToMeshIndex;
        for (int matID : matIDs) {
            matIDToMeshIndex.emplace(matID, count(matIDToMeshIndex));
        }

        std::vector<Mesh> shapeMesh(matIDs.size());

        // Loop over faces
        size_t indexOffset = 0;
        std::vector<uint32_t> indices(matIDs.size(), 0);
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);
            int materialIndex = shape.mesh.material_ids[f];

            // Loop over vertices in the face
            for (size_t v = 0; v < fv; v++) {
                Vertex vert = {};

                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
                vert.position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                vert.position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                vert.position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                if (idx.normal_index >= 0) {
                    vert.normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vert.normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vert.normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];
                }

                if (idx.texcoord_index >= 0) {
                    vert.texcoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vert.texcoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];
                }

                // Find the mesh corresponding to the material
                uint32_t meshIndex = matIDToMeshIndex.at(materialIndex);
                shapeMesh[meshIndex].vertices.push_back(vert);
                shapeMesh[meshIndex].indices.push_back(indices[meshIndex]);
                shapeMesh[meshIndex].materialIndex = materialIndex < 0 ? 0 : count(mMaterials) + materialIndex;
                shapeMesh[meshIndex].boundingBox.expand(vert.position);
                indices[meshIndex] += 1;
            }

            indexOffset += fv;
        }

        for (auto& mesh : shapeMesh) {
            mMeshes.push_back(mesh);
            newMeshIndices.push_back(count(mMeshes) - 1);
        }
    }

    // Calculate tangent space for each face (triangle)
    for (uint32_t i = 0; i < count(newMeshIndices); i++) {
        Mesh& mesh = mMeshes[newMeshIndices.at(i)];
        for (uint32_t j = 0; j < count(mesh.indices); j += 3) {
            Vertex& v0 = mesh.vertices[j + 0];
            Vertex& v1 = mesh.vertices[j + 1];
            Vertex& v2 = mesh.vertices[j + 2];

            glm::vec3 e1 = v1.position - v0.position;
            glm::vec3 e2 = v2.position - v0.position;

            glm::vec2 duv1 = v1.texcoord - v0.texcoord;
            glm::vec2 duv2 = v2.texcoord - v0.texcoord;

            float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);

            glm::vec3 t =
                glm::normalize(glm::vec3(f * (duv2.y * e1.x - duv1.y * e2.x), f * (duv2.y * e1.y - duv1.y * e2.y),
                                         f * (duv2.y * e1.z - duv1.y * e2.z)));
            glm::vec3 b =
                glm::normalize(glm::vec3(f * (-duv2.x * e1.x + duv1.x * e2.x), f * (-duv2.x * e1.y + duv1.x * e2.y),
                                         f * (-duv2.x * e1.z + duv1.x * e2.z)));

            mesh.vertices[j + 0].tangent = t;
            mesh.vertices[j + 1].tangent = t;
            mesh.vertices[j + 2].tangent = t;

            mesh.vertices[j + 0].binormal = b;
            mesh.vertices[j + 1].binormal = b;
            mesh.vertices[j + 2].binormal = b;
        }
    }

    // Remove duplicates
    for (uint32_t i = 0; i < count(newMeshIndices); i++) {
        Mesh& mesh = mMeshes[newMeshIndices.at(i)];
        std::unordered_map<Vertex, uint32_t> uniqueVertices;
        std::vector<Vertex> newVertices;
        std::vector<uint32_t> newIndices;
        uint32_t index = 0;
        for (uint32_t j = 0; j < count(mesh.indices); j++) {
            Vertex v = mesh.vertices[j];

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = index;
                newVertices.push_back(v);
                index += 1;
            }

            newIndices.push_back(uniqueVertices[v]);
        }

        mesh.vertices = newVertices;
        mesh.indices = newIndices;
    }

    // Load materials
    for (auto& material : materials) {
        Material mat;

        // tinyobjloader defaults the PBR extension fields to zero and does not report whether the file authored them,
        // so take any non-default value as the extension having been used.
        const bool pbrExtension = material.roughness > 0.0f || material.metallic > 0.0f ||
                                  !material.roughness_texname.empty() || !material.metallic_texname.empty();

        glm::vec3 diffuse(material.diffuse[0], material.diffuse[1], material.diffuse[2]);
        const glm::vec3 specular(material.specular[0], material.specular[1], material.specular[2]);
        glm::vec3 emission(material.emission[0], material.emission[1], material.emission[2]);
        float metallic = material.metallic;
        float roughness = material.roughness;

        // An MTL leaves a factor out when its map is meant to be used as it is, and tinyobjloader reports that
        // omission as zero. glTF multiplies factor by texture, so carrying the zero through would black the map out;
        // the factor the file means in that case is one.
        if (!material.diffuse_texname.empty() && diffuse == glm::vec3(0.0f)) {
            diffuse = glm::vec3(1.0f);
        }
        if (!material.emissive_texname.empty() && emission == glm::vec3(0.0f)) {
            emission = glm::vec3(1.0f);
        }
        if (!material.metallic_texname.empty() && metallic == 0.0f) {
            metallic = 1.0f;
        }
        if (!material.roughness_texname.empty() && roughness == 0.0f) {
            roughness = 1.0f;
        }

        if (pbrExtension) {
            mat.params.baseColor = diffuse;
            mat.params.metallic = metallic;
            mat.params.roughness = roughness;
        } else {
            // MTL has no metallic parameter, and Ks cannot stand in for one in general: it is a Phong highlight colour
            // that exporters routinely leave at a meaningless default such as 1 1 1, and reading that as a reflectance
            // turns every such surface into a mirror and throws Kd away. The one case where the file is unambiguous is
            // a metal authored the way metals have to be in MTL, with Kd left at black so that the whole response
            // comes from Ks.
            const float diffuseBrightness = perceivedBrightness(diffuse);
            const float specularBrightness = perceivedBrightness(specular);
            const bool metal = specularBrightness > 0.1f && diffuseBrightness < 0.1f * specularBrightness;

            mat.params.baseColor = metal ? specular : diffuse;
            mat.params.metallic = metal ? 1.0f : 0.0f;

            // Ns is a Blinn-Phong exponent, and this is the usual fit of one to a GGX roughness
            mat.params.roughness = std::clamp(std::sqrt(2.0f / (material.shininess + 2.0f)), 0.0f, 1.0f);
        }

        mat.params.alpha = material.dissolve;
        mat.params.emission = emission;

        // Ni is only meaningful from 1 upwards, and MTL files that never set it leave it there
        mat.params.indexOfRefraction = material.ior > 1.0f ? material.ior : 1.5f;

        auto setTexture = [this, &path, &materialPath, &mat](const std::string& textureName, MaterialTextureBit bit,
                                                             std::string& textureKey) {
            if (textureName.empty()) {
                mTextures.insert(std::make_pair(textureName, mpMissingTexture));
                return;
            }

            auto fullPath = std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureName);
            textureKey = fullPath.string();
            addTexture(textureKey);
            mat.params.hasTexture |= static_cast<uint32_t>(bit);
        };

        mat.params.hasTexture = 0;

        setTexture(material.diffuse_texname, MaterialTextureBit::BaseColor, mat.baseColorTexturePath);

        // MTL keeps roughness and metallic in separate maps while glTF packs them into the green and blue channels of
        // one. A grayscale map_Pr lands in the green channel correctly on its own, but a separate map_Pm cannot follow
        // it into the blue channel without combining the two images. map_Ks is no substitute, being a specular colour
        // rather than either channel, and has already been folded into the factors above.
        setTexture(material.roughness_texname, MaterialTextureBit::MetallicRoughness, mat.metallicRoughnessTexturePath);
        if (!material.metallic_texname.empty() && material.metallic_texname != material.roughness_texname) {
            Log::Warning("Material {}: map_Pm ({}) is in an image of its own and cannot be packed with map_Pr, so the "
                         "metallic factor is used instead",
                         material.name, material.metallic_texname);
        }

        setTexture(material.ambient_texname, MaterialTextureBit::Occlusion, mat.occlusionTexturePath);
        setTexture(material.emissive_texname, MaterialTextureBit::Emissive, mat.emissiveTexturePath);
        setTexture(material.normal_texname, MaterialTextureBit::Normal, mat.normalTexturePath);

        // MTL has no alpha mode. A dissolve below one asks for blending, and a base color texture that carries alpha is
        // conventionally a cutout, which is how OBJ foliage and fences are authored.
        if (material.dissolve < 1.0f) {
            mat.params.alphaMode = static_cast<uint32_t>(AlphaMode::Blend);
        } else if (mat.params.hasTexture & static_cast<uint32_t>(MaterialTextureBit::BaseColor)) {
            mat.params.alphaMode = static_cast<uint32_t>(AlphaMode::Mask);
        }

        mMaterials.push_back(mat);
    }

    return newMeshIndices;
}

template <typename T>
static void readCastInsertIndices(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
                                  std::vector<uint32_t>& indices)
{
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    const T* byteIndices = reinterpret_cast<const T*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
    std::transform(byteIndices, byteIndices + accessor.count, std::back_inserter(indices),
                   [](T index) { return static_cast<uint32_t>(index); });
}

std::vector<uint32_t> Scene::loadFromGLTF(const std::filesystem::path& path)
{
    std::vector<uint32_t> newMeshIndices;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    if (path.extension() == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    } else if (path.extension() == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
    }

    if (!warn.empty()) {
        Log::Warning("TinyGLTF: {}", warn);
    }

    if (!err.empty()) {
        Log::Error("TinyGLTF: {}", err);
    }

    if (!ret) {
        Log::Error("Failed to load {}", path.string());
        return {};
    }

    // Loop over meshes
    for (const auto& mesh : model.meshes) {
        // Loop over primitives in the mesh
        for (const auto& primitive : mesh.primitives) {
            Mesh newMesh;
            // Get vertex attributes
            if (primitive.attributes.count("POSITION") > 0) {
                const auto& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                const float* positions =
                    reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                for (size_t i = 0; i < accessor.count; i++) {
                    Vertex vertex;
                    vertex.position.x = positions[i * 3 + 0];
                    vertex.position.y = positions[i * 3 + 1];
                    vertex.position.z = positions[i * 3 + 2];
                    newMesh.vertices.push_back(vertex);
                }
                std::vector<glm::vec3> vertexPositions;
                for (const auto& vertex : newMesh.vertices) {
                    vertexPositions.push_back(vertex.position);
                }
                newMesh.boundingBox = AABB::calculate(vertexPositions);
            }
            if (primitive.attributes.count("NORMAL") > 0) {
                const auto& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                const float* normals =
                    reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                for (size_t i = 0; i < accessor.count; i++) {
                    newMesh.vertices[i].normal.x = normals[i * 3 + 0];
                    newMesh.vertices[i].normal.y = normals[i * 3 + 1];
                    newMesh.vertices[i].normal.z = normals[i * 3 + 2];
                }
            }
            if (primitive.attributes.count("TEXCOORD_0") > 0) {
                const auto& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                const float* texcoords =
                    reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                for (size_t i = 0; i < accessor.count; i++) {
                    newMesh.vertices[i].texcoord.x = texcoords[i * 2 + 0];
                    newMesh.vertices[i].texcoord.y = 1.0f - texcoords[i * 2 + 1];
                }
            }

            // Get indices
            std::vector<uint32_t> indices;
            if (primitive.indices >= 0) {
                const auto& accessor = model.accessors[primitive.indices];
                switch (accessor.componentType) {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    readCastInsertIndices<uint8_t>(model, accessor, indices);
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    readCastInsertIndices<uint16_t>(model, accessor, indices);
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                    readCastInsertIndices<uint32_t>(model, accessor, indices);
                }
                }
            } else {
                // No indices, use sequential
                indices.resize(count(newMesh.vertices));
                std::iota(indices.begin(), indices.end(), 0);
            }
            newMesh.indices.insert(newMesh.indices.end(), indices.begin(), indices.end());

            // Material index. A primitive without a material uses the scene default, which sits at index 0.
            newMesh.materialIndex = primitive.material < 0 ? 0 : count(mMaterials) + primitive.material;

            // And push the new mesh
            mMeshes.push_back(newMesh);
            newMeshIndices.push_back(count(mMeshes) - 1);
        }
    }

    // Calculate tangent space for each face (triangle)
    for (uint32_t i = 0; i < count(newMeshIndices); i++) {
        Mesh& mesh = mMeshes[newMeshIndices.at(i)];
        for (uint32_t j = 0; j < count(mesh.indices); j += 3) {
            Vertex& v0 = mesh.vertices[mesh.indices[j + 0]];
            Vertex& v1 = mesh.vertices[mesh.indices[j + 1]];
            Vertex& v2 = mesh.vertices[mesh.indices[j + 2]];

            glm::vec3 e1 = v1.position - v0.position;
            glm::vec3 e2 = v2.position - v0.position;

            glm::vec2 duv1 = v1.texcoord - v0.texcoord;
            glm::vec2 duv2 = v2.texcoord - v0.texcoord;

            float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);

            glm::vec3 t =
                glm::normalize(glm::vec3(f * (duv2.y * e1.x - duv1.y * e2.x), f * (duv2.y * e1.y - duv1.y * e2.y),
                                         f * (duv2.y * e1.z - duv1.y * e2.z)));
            glm::vec3 b =
                glm::normalize(glm::vec3(f * (-duv2.x * e1.x + duv1.x * e2.x), f * (-duv2.x * e1.y + duv1.x * e2.y),
                                         f * (-duv2.x * e1.z + duv1.x * e2.z)));

            mesh.vertices[mesh.indices[j + 0]].tangent = t;
            mesh.vertices[mesh.indices[j + 1]].tangent = t;
            mesh.vertices[mesh.indices[j + 2]].tangent = t;

            mesh.vertices[mesh.indices[j + 0]].binormal = b;
            mesh.vertices[mesh.indices[j + 1]].binormal = b;
            mesh.vertices[mesh.indices[j + 2]].binormal = b;
        }
    }

    // Helper to get extension values, falling back to what the extension defines the value to be when absent
    auto getExtensionValue = [](const tinygltf::Material& material, const std::string& extensionName,
                                const std::string& key, double fallback) -> double {
        if (material.extensions.find(extensionName) != material.extensions.end()) {
            const auto& extension = material.extensions.at(extensionName);
            if (extension.IsObject() && extension.Has(key)) {
                return extension.Get(key).Get<double>();
            }
        }
        return fallback;
    };

    // Load materials
    for (auto& material : model.materials) {
        Material mat;
        const auto& pbr = material.pbrMetallicRoughness;

        mat.params.baseColor.r = static_cast<float>(pbr.baseColorFactor[0]);
        mat.params.baseColor.g = static_cast<float>(pbr.baseColorFactor[1]);
        mat.params.baseColor.b = static_cast<float>(pbr.baseColorFactor[2]);
        mat.params.alpha = static_cast<float>(pbr.baseColorFactor[3]);

        mat.params.metallic = static_cast<float>(pbr.metallicFactor);
        mat.params.roughness = static_cast<float>(pbr.roughnessFactor);

        mat.params.normalScale = static_cast<float>(material.normalTexture.scale);
        mat.params.occlusionStrength = static_cast<float>(material.occlusionTexture.strength);

        mat.params.emission.r = static_cast<float>(material.emissiveFactor[0]);
        mat.params.emission.g = static_cast<float>(material.emissiveFactor[1]);
        mat.params.emission.b = static_cast<float>(material.emissiveFactor[2]);
        mat.params.emissiveStrength =
            static_cast<float>(getExtensionValue(material, "KHR_materials_emissive_strength", "emissiveStrength", 1.0));

        mat.params.indexOfRefraction = static_cast<float>(getExtensionValue(material, "KHR_materials_ior", "ior", 1.5));
        mat.params.transmission =
            static_cast<float>(getExtensionValue(material, "KHR_materials_transmission", "transmissionFactor", 0.0));

        mat.params.alphaCutoff = static_cast<float>(material.alphaCutoff);
        if (material.alphaMode == "MASK") {
            mat.params.alphaMode = static_cast<uint32_t>(AlphaMode::Mask);
        } else if (material.alphaMode == "BLEND") {
            mat.params.alphaMode = static_cast<uint32_t>(AlphaMode::Blend);
        }

        // Captured by reference, the model in particular: copying it would deep copy every buffer in the file, once
        // per material
        auto setTexture = [this, &path, &model](std::unordered_map<std::string, ptr<Texture>>& loadedTextures,
                                                int textureIndex, ptr<Texture> pMissingTexture,
                                                std::string& textureKey) {
            if (textureIndex >= 0) {
                std::string textureName = model.images[model.textures[textureIndex].source].uri;
                if (textureName.empty()) {
                    // Image is stored in buffer view
                    const auto& bufferView =
                        model.bufferViews[model.images[model.textures[textureIndex].source].bufferView];
                    const auto& buffer = model.buffers[bufferView.buffer];
                    const uint8_t* pFileData = buffer.data.data() + bufferView.byteOffset;

                    textureName = model.images[model.textures[textureIndex].source].name;
                    if (textureName.empty()) {
                        // Generate a unique name
                        static uint32_t unnamedTextureCount = 0;
                        textureName = "unnamed_texture_" + std::to_string(unnamedTextureCount++);
                    }
                    textureKey = textureName;
                    addTextureFromMemory(pFileData, bufferView.byteLength, textureName);

                    return true;
                }
                auto fullPath = std::filesystem::canonical(path.parent_path() / textureName);
                textureKey = fullPath.string();
                addTexture(textureKey);
                return true;
            }
            loadedTextures.insert(std::make_pair("", pMissingTexture));
            return false;
        };

        mat.params.hasTexture = 0;

        if (setTexture(mTextures, pbr.baseColorTexture.index, mpMissingTexture, mat.baseColorTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::BaseColor);
        }
        if (setTexture(mTextures, pbr.metallicRoughnessTexture.index, mpMissingTexture,
                       mat.metallicRoughnessTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::MetallicRoughness);
        }
        if (setTexture(mTextures, material.occlusionTexture.index, mpMissingTexture, mat.occlusionTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Occlusion);
        }
        if (setTexture(mTextures, material.emissiveTexture.index, mpMissingTexture, mat.emissiveTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Emissive);
        }
        if (setTexture(mTextures, material.normalTexture.index, mpMissingTexture, mat.normalTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Normal);
        }

        mMaterials.push_back(mat);
    }

    return newMeshIndices;
}

void Scene::addTexture(std::string texturePath)
{
    if (texturePath.empty()) {
        return;
    }

    if (mTextures.contains(texturePath)) {
        return;
    }

    bool generateMipmaps = true;
    auto pTexture =
        mpDevice->createTextureFromFile(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, texturePath, generateMipmaps);

    if (pTexture != nullptr) {
        mTextures.insert(std::make_pair(texturePath, pTexture));
    } else {
        mTextures.insert(std::make_pair(texturePath, mpMissingTexture));
    }
}

void Scene::addTextureFromMemory(const uint8_t* pFileData, size_t size, const std::string& textureName)
{
    if (mTextures.contains(textureName)) {
        return;
    }

    int width;
    int height;
    int channels;

    stbi_set_flip_vertically_on_load(1);
    stbi_uc* pData =
        stbi_load_from_memory(pFileData, static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha);

    const uint32_t bytesPerPixel = 4;
    const bool generateMipmaps = true;
    auto pTexture = mpDevice->createTextureFromBuffer(TextureType::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, pData, width,
                                                      height, 1, bytesPerPixel, generateMipmaps);

    if (pTexture != nullptr) {
        mTextures.insert(std::make_pair(textureName, pTexture));
    } else {
        mTextures.insert(std::make_pair(textureName, mpMissingTexture));
    }
}
