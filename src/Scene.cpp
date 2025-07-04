#include "Scene.h"

#include "Extension.h"
#include "Helpers.h"
#include "Log.h"
#include "Pipeline.h"

#include "tiny_obj_loader.h"
#include "tinygltf/tiny_gltf.h"

using namespace Mandrill;

enum class MaterialTextureBit : uint32_t {
    Diffuse = 1 << 0,
    Specular = 1 << 1,
    Ambient = 1 << 2,
    Emission = 1 << 3,
    Normal = 1 << 4,
};

Node::Node()
{
    mTransform = glm::identity<glm::mat4>();
    mVisible = true;
    mpTransformDevice = nullptr;
}

Node::~Node()
{
}

void Node::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, const ptr<const Scene> pScene) const
{
    if (!mVisible || !mpPipeline) {
        return;
    }

    std::memcpy(mpTransformDevice + pScene->mpSwapchain->getInFlightIndex(), &mTransform, sizeof(glm::mat4));

    mpPipeline->bind(cmd);

    // Bind descriptor set for camera matrices, node transform, and environment map
    VkDeviceSize alignment = pScene->mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDescriptorOffset = static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) *
                                                            pScene->mpSwapchain->getInFlightIndex());
    pCamera->getDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 0,
                                   cameraDescriptorOffset);

    uint32_t nodeDescriptorOffset =
        static_cast<uint32_t>(Helpers::alignTo(sizeof(glm::mat4), alignment) * pScene->mpSwapchain->getInFlightIndex());
    pDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 1, nodeDescriptorOffset);

    if (pScene->mpEnvironmentMapDescriptor) {
        pScene->mpEnvironmentMapDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 3);
    }

    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];

        // Bind descriptor set for material
        pScene->mMaterials[mesh.materialIndex].pDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                                 mpPipeline->getLayout(), 2);

        // Bind vertex and index buffers
        std::array<VkBuffer, 1> vertexBuffers = {pScene->mpVertexBuffer->getBuffer()};
        std::array<VkDeviceSize, 1> offsets = {mesh.deviceVerticesOffset};
        vkCmdBindVertexBuffers(cmd, 0, count(vertexBuffers), vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(cmd, pScene->mpIndexBuffer->getBuffer(), mesh.deviceIndicesOffset, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, count(mesh.indices), 1, 0, 0, 0);
    }
}

Scene::Scene(ptr<Device> pDevice, ptr<Swapchain> pSwapchain, bool supportRayTracing)
    : mpDevice(pDevice), mpSwapchain(pSwapchain), mSupportRayTracing(supportRayTracing), mVertexCount(0), mIndexCount(0)
{
    const uint8_t data[] = {0xff, 0x00, 0xff, 0xff, 0x88, 0x00, 0xff, 0xff,
                            0x88, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff};
    mpMissingTexture =
        make_ptr<Texture>(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, data, 2, 2, 1, 4, false);
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd, const ptr<Camera> pCamera) const
{
    for (auto& node : mNodes) {
        node.render(cmd, pCamera, shared_from_this());
    }
}

ptr<Node> Scene::addNode()
{
    Node node = {};

    mNodes.push_back(node);

    return ptr<Node>(&*(mNodes.end() - 1), [](Node*) {});
}

uint32_t Scene::addMaterial(Material material)
{
    auto setTexture = [this](std::unordered_map<std::string, ptr<Texture>>& loadedTextures, std::string texturePath,
                             MaterialTextureBit bit, ptr<Texture> pMissingTexture) {
        if (!texturePath.empty()) {
            addTexture(texturePath);
        } else {
            loadedTextures.insert(std::make_pair(texturePath, pMissingTexture));
        }
    };

    material.params.hasTexture = 0;

    setTexture(mTextures, material.diffuseTexturePath, MaterialTextureBit::Diffuse, mpMissingTexture);
    setTexture(mTextures, material.specularTexturePath, MaterialTextureBit::Specular, mpMissingTexture);
    setTexture(mTextures, material.ambientTexturePath, MaterialTextureBit::Ambient, mpMissingTexture);
    setTexture(mTextures, material.emissionTexturePath, MaterialTextureBit::Emission, mpMissingTexture);
    setTexture(mTextures, material.normalTexturePath, MaterialTextureBit::Normal, mpMissingTexture);

    mMaterials.push_back(material);

    return count(mMaterials) - 1;
}

uint32_t Scene::addMesh(const std::vector<Vertex> vertices, const std::vector<uint32_t> indices, uint32_t materialIndex)
{
    Mesh mesh = {
        .vertices = vertices,
        .indices = indices,
        .materialIndex = materialIndex,
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
        newMeshIndices = loadFromGLTF(path, materialPath);
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
        make_ptr<Buffer>(mpDevice, verticesSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpIndexBuffer =
        make_ptr<Buffer>(mpDevice, indicesSize,
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                             VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uint32_t copies = mpSwapchain->getFramesInFlightCount();

    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;

    // Transforms can change between frames, material parameters can not
    VkDeviceSize transformsSize = Helpers::alignTo(sizeof(glm::mat4), alignment) * mNodes.size() * copies;
    mpTransforms = make_ptr<Buffer>(mpDevice, transformsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize materialParamsSize = Helpers::alignTo(sizeof(MaterialParams), alignment) * mMaterials.size();
    mpMaterialParams = make_ptr<Buffer>(mpDevice, materialParamsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Associate each node with a part of the transforms buffer, and with multiple copies for each frame in flight
    glm::mat4* transforms = static_cast<glm::mat4*>(mpTransforms->getHostMap());
    for (uint32_t i = 0; i < count(mNodes); i++) {
        mNodes[i].mpTransformDevice = transforms + i * copies;
        for (uint32_t c = 0; c < copies; c++) {
            *(mNodes[i].mpTransformDevice + c) = glm::mat4(1.0f);
        }
    }

    // Associate each material with a part of the material params buffer
    MaterialParams* materialParams = static_cast<MaterialParams*>(mpMaterialParams->getHostMap());
    for (uint32_t i = 0; i < count(mMaterials); i++) {
        mMaterials[i].paramsDevice = materialParams + i;
        *mMaterials[i].paramsDevice = mMaterials[i].params;
        mMaterials[i].paramsOffset = Helpers::alignTo(i * sizeof(MaterialParams), alignment);
    }

    if (mSupportRayTracing) {
        VkDeviceSize materialBufferSize = sizeof(MaterialDevice) * mMaterials.size();
        mpMaterialBuffer = make_ptr<Buffer>(mpDevice, materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        MaterialDevice* materials = static_cast<MaterialDevice*>(mpMaterialBuffer->getHostMap());
        for (uint32_t i = 0; i < count(mMaterials); i++) {
            memcpy(&materials[i].params, materialParams + i, sizeof(MaterialParams));
            materials[i].diffuseTextureIndex = static_cast<uint32_t>(
                std::distance(mTextures.begin(), mTextures.find(mMaterials[i].diffuseTexturePath)));
            materials[i].specularTextureIndex = static_cast<uint32_t>(
                std::distance(mTextures.begin(), mTextures.find(mMaterials[i].specularTexturePath)));
            materials[i].ambientTextureIndex = static_cast<uint32_t>(
                std::distance(mTextures.begin(), mTextures.find(mMaterials[i].ambientTexturePath)));
            materials[i].emissionTextureIndex = static_cast<uint32_t>(
                std::distance(mTextures.begin(), mTextures.find(mMaterials[i].emissionTexturePath)));
            materials[i].normalTextureIndex = static_cast<uint32_t>(
                std::distance(mTextures.begin(), mTextures.find(mMaterials[i].normalTexturePath)));
        }

        VkDeviceSize instanceDataBufferSize = sizeof(InstanceData) * mMeshes.size();
        mpInstanceDataBuffer = make_ptr<Buffer>(mpDevice, instanceDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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

    if (!mSupportRayTracing) {
        createDescriptors();
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

void Scene::updateAccelerationStructure(VkBuildAccelerationStructureFlagsKHR flags)
{
    if (!mSupportRayTracing) {
        Log::Error("Cannot build acceleration structure for a scene that was not initiated to support ray tracing. Set "
                   "supportRayTracing to TRUE during scene creation.");
        return;
    }

    if (!mpAccelerationStructure) {
        mpAccelerationStructure = make_ptr<AccelerationStructure>(mpDevice, shared_from_this(), flags);

        // After acceleration structure is built, we can create the descriptors
        createDescriptors();

        // The acceleration structure will be built during the creation so we can return here
        return;
    }

    mpAccelerationStructure->update(flags);
}

void Scene::bindRayTracingDescriptors(VkCommandBuffer cmd, ptr<Camera> pCamera, VkPipelineLayout layout)
{
    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDescriptorOffset =
        static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) * mpSwapchain->getInFlightIndex());
    pCamera->getDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 0, cameraDescriptorOffset);

    if (mpEnvironmentMapDescriptor) {
        mpEnvironmentMapDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 3);
    }

    mpRayTracingDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 4);
}

void Scene::setSampler(const ptr<Sampler> pSampler)
{
    mpMissingTexture->setSampler(pSampler);

    for (auto& texture : mTextures) {
        texture.second->setSampler(pSampler);
    }
}

ptr<Layout> Scene::getLayout()
{
    std::vector<LayoutDesc> desc;

    // 0.0: Camera matrices
    // 1.0: Model matrix
    // 2.0: Material parameters
    // 2.1: Material diffuse texture
    // 2.2: Material specular texture
    // 2.3: Material ambient texture
    // 2.4: Material emission texture
    // 2.5: Material normal texture
    // 3.0: Environment map texture
    desc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                      VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
    desc.emplace_back(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(2, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS);
    desc.emplace_back(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_MISS_BIT_KHR);

    if (mSupportRayTracing) {
        // 4.0: Acceleration structure
        // 4.1: Scene vertex buffer
        // 4.2: Scene index buffer
        // 4.3: Scene material buffer
        // 4.4: Scene texture array
        // 4.5: Instance data buffer
        // 5.0: Output storage image
        desc.emplace_back(4, 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        desc.emplace_back(4, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        desc.emplace_back(4, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        desc.emplace_back(4, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        desc.emplace_back(4, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                          count(mTextures));
        desc.emplace_back(4, 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
        desc.emplace_back(5, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_ALL);
    }

    return make_ptr<Layout>(mpDevice, desc);
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
        // One mesh per material in shape
        std::set<int> matIDs;
        for (int matID : shape.mesh.material_ids) {
            matIDs.insert(matID);
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

                auto meshIndex = std::distance(matIDs.find(materialIndex), matIDs.end()) - 1;
                shapeMesh[meshIndex].vertices.push_back(vert);
                shapeMesh[meshIndex].indices.push_back(indices[meshIndex]);
                shapeMesh[meshIndex].materialIndex = count(mMaterials) + materialIndex;
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
        mat.params.diffuse.r = material.diffuse[0];
        mat.params.diffuse.g = material.diffuse[1];
        mat.params.diffuse.b = material.diffuse[2];

        mat.params.specular.r = material.specular[0];
        mat.params.specular.g = material.specular[1];
        mat.params.specular.b = material.specular[2];

        mat.params.ambient.r = material.ambient[0];
        mat.params.ambient.g = material.ambient[1];
        mat.params.ambient.b = material.ambient[2];

        mat.params.emission.r = material.emission[0];
        mat.params.emission.g = material.emission[1];
        mat.params.emission.b = material.emission[2];

        mat.params.shininess = material.shininess;
        mat.params.indexOfRefraction = material.ior;
        mat.params.opacity = material.dissolve;

        auto setTexture = [this, path, materialPath](std::unordered_map<std::string, ptr<Texture>>& loadedTextures,
                                                     std::string textureName, ptr<Texture> pMissingTexture,
                                                     std::string& textureKey) {
            if (!textureName.empty()) {
                auto fullPath =
                    std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureName);
                textureKey = fullPath.string();
                addTexture(textureKey);
                return true;
            }

            loadedTextures.insert(std::make_pair(textureName, pMissingTexture));
            return false;
        };

        mat.params.hasTexture = 0;

        if (setTexture(mTextures, material.diffuse_texname, mpMissingTexture, mat.diffuseTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Diffuse);
        }
        if (setTexture(mTextures, material.specular_texname, mpMissingTexture, mat.specularTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Specular);
        }
        if (setTexture(mTextures, material.ambient_texname, mpMissingTexture, mat.ambientTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Ambient);
        }
        if (setTexture(mTextures, material.emissive_texname, mpMissingTexture, mat.emissionTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Emission);
        }
        if (setTexture(mTextures, material.normal_texname, mpMissingTexture, mat.normalTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Normal);
        }

        mMaterials.push_back(mat);
    }

    return newMeshIndices;
}

std::vector<uint32_t> Scene::loadFromGLTF(const std::filesystem::path& path, const std::filesystem::path& materialPath)
{
    // std::vector<uint32_t> newMeshIndices;

    // tinygltf::Model model;
    // tinygltf::TinyGLTF loader;
    // std::string err;
    // std::string warn;

    // bool ret = false;
    // if (path.extension() == ".gltf") {
    //     ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    // } else if (path.extension() == ".glb") {
    //     ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
    // }

    // if (!warn.empty()) {
    //     Log::Warning("TinyGLTF: {}", warn);
    // }

    // if (!err.empty()) {
    //     Log::Error("TinyGLTF: {}", err);
    // }

    // if (!ret) {
    //     Log::Error("Failed to load {}", path.string());
    //     return {};
    // }

    //// Loop over meshes
    // for (const auto& mesh : model.meshes) {
    //     // Loop over primitives in the mesh
    //     for (const auto& primitive : mesh.primitives) {
    //         Mesh newMesh;
    //         // Get material index
    //         if (primitive.material >= 0 && primitive.material < count(model.materials)) {
    //             newMesh.materialIndex = addMaterial(model.materials[primitive.material]);
    //         } else {
    //             newMesh.materialIndex = 0; // Default material
    //         }
    //         // Get vertex attributes
    //         if (primitive.attributes.count("POSITION") > 0) {
    //             const auto& accessor = model.accessors[primitive.attributes.at("POSITION")];
    //             const auto& bufferView = model.bufferViews[accessor.bufferView];
    //             const auto& buffer = model.buffers[bufferView.buffer];
    //             const float* positions =
    //                 reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
    //             for (size_t i = 0; i < accessor.count; i++) {
    //                 Vertex vertex;
    //                 vertex.position.x = positions[i * 3 + 0];
    //                 vertex.position.y = positions[i * 3 + 1];
    //                 vertex.position.z = positions[i * 3 + 2];
    //                 newMesh.vertices.push_back(vertex);
    //             }
    //         }
    //         if (primitive.attributes.count("NORMAL") > 0) {
    //             const auto& accessor = model.accessors[primitive.attributes.at("NORMAL")];
    //             const auto& bufferView = model.bufferViews[accessor.bufferView];
    //             const auto& buffer = model.buffers[bufferView.buffer];
    //             const float* normals =
    //                 reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
    //             for (size_t i = 0; i < accessor.count; i++) {
    //                 newMesh.vertices[i].normal.x = normals[i * 3 + 0];
    //                 newMesh.vertices[i].normal.y = normals[i * 3 + 1];
    //                 newMesh.vertices[i].normal.z = normals[i * 3 + 2];
    //             }
    //         }
    //         if (primitive.attributes.count("TEXCOORD_0") > 0) {
    //             const auto& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
    //             const auto& bufferView = model.bufferViews[accessor.bufferView];
    //             const auto& buffer = model.buffers[bufferView.buffer];
    //             const float* texcoords =
    //                 reinterpret_cast<const float*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
    //         }
    //     }
    // }

    // return newMeshIndices;
    return {};
}

void Scene::addTexture(std::string texturePath)
{
    if (texturePath.empty()) {
        return;
    }

    if (mTextures.contains(texturePath)) {
        return;
    }

    auto pTexture = make_ptr<Texture>(mpDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, texturePath, true);
    mTextures.insert(std::make_pair(texturePath, pTexture));
}

void Scene::createDescriptors()
{
    ptr<Layout> pLayout = getLayout();

    // Node transforms
    VkDeviceSize offset = 0;
    for (auto& node : mNodes) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, mpTransforms);
        desc.back().range = sizeof(glm::mat4);
        desc.back().offset = offset;

        offset += Helpers::alignTo(sizeof(glm::mat4),
                                   mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment) *
                  mpSwapchain->getFramesInFlightCount();

        // Set layout for set 1
        auto layout = pLayout->getDescriptorSetLayouts()[1];
        node.pDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout);
    }

    // Materials
    for (auto& mat : mMaterials) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mpMaterialParams);
        desc.back().offset = mat.paramsOffset;
        desc.back().range = sizeof(MaterialParams);

        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.diffuseTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.specularTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.ambientTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.emissionTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.normalTexturePath]);

        // Set layout for set 2
        auto layout = pLayout->getDescriptorSetLayouts()[2];
        mat.pDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout);
    }

    // Environment map
    if (mpEnvironmentMap) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpEnvironmentMap);

        // Set layout for set 3
        auto layout = pLayout->getDescriptorSetLayouts()[3];
        mpEnvironmentMapDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout);
    }

    // Add extra descriptors for ray tracing (set 4)
    if (mSupportRayTracing) {
        std::vector<DescriptorDesc> desc;

        // Get a list of the textures
        std::vector<ptr<Texture>> textures;
        std::transform(mTextures.begin(), mTextures.end(), std::back_inserter(textures),
                       [](const auto& entry) { return entry.second; });
        ptr<std::vector<ptr<Texture>>> pTextures = make_ptr<std::vector<ptr<Texture>>>(textures);

        desc.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, mpAccelerationStructure);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpVertexBuffer);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpIndexBuffer);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpMaterialBuffer);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pTextures, 0, 0, count(textures));
        desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpInstanceDataBuffer);

        auto layout = pLayout->getDescriptorSetLayouts()[4];
        mpRayTracingDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout);
    }
}
