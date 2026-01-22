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

void Node::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, uint32_t frameInFlightIndex,
                  const ptr<const Scene> pScene) const
{
    if (!mVisible || !mpPipeline) {
        return;
    }

    std::memcpy(mpTransformDevice + frameInFlightIndex, &mTransform, sizeof(glm::mat4));

    mpPipeline->bind(cmd);

    // Bind descriptor set for node transform
    VkDeviceSize alignment = pScene->mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t nodeDescriptorOffset =
        static_cast<uint32_t>(Helpers::alignTo(sizeof(glm::mat4), alignment) * frameInFlightIndex);

#ifdef _DEBUG
    if (!mpDescriptor) {
        Log::Error("Node::render() - No descriptor set bound to node. When using ray tracing, descriptors are not "
                   "created until the acceleration structure has been built.");
        return;
    }
#endif

    mpDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 1, nodeDescriptorOffset);

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
    mMaterials.push_back({});
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, uint32_t frameInFlightIndex) const
{
    if (mNodes.empty()) {
        return;
    }

    // Bind descriptor set for camera matrices and environment map
    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDescriptorOffset =
        static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) * frameInFlightIndex);
    pCamera->getDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mNodes[0].mpPipeline->getLayout(), 0,
                                   cameraDescriptorOffset);

    if (mpEnvironmentMapDescriptor) {
        mpEnvironmentMapDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mNodes[0].mpPipeline->getLayout(), 3);
    }

    for (auto& node : mNodes) {
        node.render(cmd, pCamera, frameInFlightIndex, shared_from_this());
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

void Scene::compile(uint32_t frameInFlightCount)
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

    // Transforms can change between frames, material parameters can not
    VkDeviceSize transformsSize = Helpers::alignTo(sizeof(glm::mat4), alignment) * mNodes.size() * frameInFlightCount;
    mpTransforms = mpDevice->createBuffer(transformsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize materialParamsSize = Helpers::alignTo(sizeof(MaterialParams), alignment) * mMaterials.size();
    mpMaterialParams = mpDevice->createBuffer(materialParamsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Associate each node with a part of the transforms buffer, and with multiple copies for each frame in flight
    glm::mat4* transforms = static_cast<glm::mat4*>(mpTransforms->getHostMap());
    for (uint32_t i = 0; i < count(mNodes); i++) {
        mNodes[i].mpTransformDevice = transforms + i * frameInFlightCount;
        for (uint32_t c = 0; c < frameInFlightCount; c++) {
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

    // For ray tracing a global list is used and this struct keeps track of the texture indices
    VkDeviceSize materialBufferSize = sizeof(MaterialDevice) * mMaterials.size();
    mpMaterialBuffer = mpDevice->createBuffer(materialBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    MaterialDevice* materials = static_cast<MaterialDevice*>(mpMaterialBuffer->getHostMap());
    for (uint32_t i = 0; i < count(mMaterials); i++) {
        memcpy(&materials[i].params, materialParams + i, sizeof(MaterialParams));
        materials[i].diffuseTextureIndex =
            static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(mMaterials[i].diffuseTexturePath)));
        materials[i].specularTextureIndex =
            static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(mMaterials[i].specularTexturePath)));
        materials[i].ambientTextureIndex =
            static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(mMaterials[i].ambientTexturePath)));
        materials[i].emissionTextureIndex =
            static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(mMaterials[i].emissionTexturePath)));
        materials[i].normalTextureIndex =
            static_cast<uint32_t>(std::distance(mTextures.begin(), mTextures.find(mMaterials[i].normalTexturePath)));
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

void Scene::createDescriptors(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                              uint32_t frameInFlightCount)
{
    // Node transforms
    VkDeviceSize offset = 0;
    for (auto& node : mNodes) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, mpTransforms);
        desc.back().range = sizeof(glm::mat4);
        desc.back().offset = offset;

        offset += Helpers::alignTo(sizeof(glm::mat4),
                                   mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment) *
                  frameInFlightCount;

        // Set layout for set 1
        node.mpDescriptor = mpDevice->createDescriptor(desc, descriptorSetLayouts[1]);
    }

    // Materials
    for (auto& mat : mMaterials) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mpMaterialParams, mat.paramsOffset,
                          sizeof(MaterialParams));
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.diffuseTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.specularTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.ambientTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.emissionTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.normalTexturePath]);

        // Set layout for set 2
        mat.pDescriptor = mpDevice->createDescriptor(desc, descriptorSetLayouts[2]);
    }

    // Environment map
    if (mpEnvironmentMap) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpEnvironmentMap);

        // Set layout for set 3
        mpEnvironmentMapDescriptor = mpDevice->createDescriptor(desc, descriptorSetLayouts[3]);
    }
}

void Scene::createRayTracingDescriptors(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                        const ptr<AccelerationStructure> pAccelerationStructure,
                                        uint32_t frameInFlightCount)
{
    // Get a list of the textures
    std::vector<ptr<Texture>> textures;
    std::transform(mTextures.begin(), mTextures.end(), std::back_inserter(textures),
                   [](const auto& entry) { return entry.second; });
    ptr<std::vector<ptr<Texture>>> pTextures = make_ptr<std::vector<ptr<Texture>>>(textures);

    std::vector<DescriptorDesc> desc;
    desc.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, pAccelerationStructure);
    desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpVertexBuffer);
    desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpIndexBuffer);
    desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpInstanceDataBuffer);
    desc.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mpMaterialBuffer);
    desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, pTextures, count(textures));

    mpRayTracingDescriptor = mpDevice->createDescriptor(desc, descriptorSetLayouts[1]);

    // Environment map
    if (mpEnvironmentMap) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mpEnvironmentMap);

        // Set layout for set 2
        mpEnvironmentMapDescriptor = mpDevice->createDescriptor(desc, descriptorSetLayouts[2]);
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

void Scene::bindRayTracingDescriptors(VkCommandBuffer cmd, ptr<Camera> pCamera, VkPipelineLayout layout,
                                      uint32_t frameInFlightIndex)
{
    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t cameraDescriptorOffset =
        static_cast<uint32_t>(Helpers::alignTo(sizeof(CameraMatrices), alignment) * frameInFlightIndex);
    pCamera->getRayTracingDescriptor()->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 0,
                                             cameraDescriptorOffset);

    mpRayTracingDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 1);

    if (mpEnvironmentMapDescriptor) {
        mpEnvironmentMapDescriptor->bind(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, layout, 2);
    }
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

                // Find the mesh corresponding to the material
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

template <typename T>
void readCastInsertIndices(tinygltf::Model model, tinygltf::Accessor accessor, std::vector<uint32_t>& indices)
{
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    const T* byteIndices = reinterpret_cast<const T*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
    std::transform(byteIndices, byteIndices + accessor.count, std::back_inserter(indices),
                   [](T index) { return static_cast<uint32_t>(index); });
}

std::vector<uint32_t> Scene::loadFromGLTF(const std::filesystem::path& path, const std::filesystem::path& materialPath)
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

            // Material index
            newMesh.materialIndex = count(mMaterials) + primitive.material;

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

    // Remove duplicates
    for (uint32_t i = 0; i < count(newMeshIndices); i++) {
        Mesh& mesh = mMeshes[newMeshIndices.at(i)];
        std::unordered_map<Vertex, uint32_t> uniqueVertices;
        std::vector<Vertex> newVertices;
        std::vector<uint32_t> newIndices;
        uint32_t index = 0;
        for (uint32_t j = 0; j < count(mesh.indices); j++) {
            Vertex v = mesh.vertices[mesh.indices[j]];

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

    auto getExtensionValue = [](const tinygltf::Material& material, const std::string& extensionName,
                                const std::string& key) -> double {
        if (material.extensions.find(extensionName) != material.extensions.end()) {
            const auto& extension = material.extensions.at(extensionName);
            if (extension.IsObject() && extension.Has(key)) {
                return extension.Get(key).Get<double>();
            }
        }
        return 0.0;
    };

    // Load materials
    for (auto& material : model.materials) {
        Material mat;
        mat.params.diffuse.r = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]);
        mat.params.diffuse.g = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]);
        mat.params.diffuse.b = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]);

        mat.params.specular.r = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
        mat.params.specular.g = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
        mat.params.specular.b = 0.0f;

        mat.params.ambient.r = 0.0f;
        mat.params.ambient.g = 0.0f;
        mat.params.ambient.b = 0.0f;

        mat.params.emission.r = static_cast<float>(material.emissiveFactor[0]);
        mat.params.emission.g = static_cast<float>(material.emissiveFactor[0]);
        mat.params.emission.b = static_cast<float>(material.emissiveFactor[0]);

        mat.params.shininess = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
        mat.params.indexOfRefraction = static_cast<float>(getExtensionValue(material, "KHR_materials_ior", "ior"));
        mat.params.opacity =
            1.0f - static_cast<float>(getExtensionValue(material, "KHR_materials_transmission", "transmissionFactor"));

        auto setTexture = [this, path, materialPath,
                           model](std::unordered_map<std::string, ptr<Texture>>& loadedTextures, int textureIndex,
                                  ptr<Texture> pMissingTexture, std::string& textureKey) {
            if (textureIndex >= 0) {
                std::string textureName = model.images[model.textures[textureIndex].source].uri;
                auto fullPath =
                    std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureName);
                textureKey = fullPath.string();
                addTexture(textureKey);
                return true;
            }
            loadedTextures.insert(std::make_pair("", pMissingTexture));
            return false;
        };

        mat.params.hasTexture = 0;

        if (setTexture(mTextures, material.pbrMetallicRoughness.baseColorTexture.index, mpMissingTexture,
                       mat.diffuseTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Diffuse);
        }
        if (setTexture(mTextures, material.pbrMetallicRoughness.metallicRoughnessTexture.index, mpMissingTexture,
                       mat.specularTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Specular);
        }
        if (setTexture(mTextures, material.occlusionTexture.index, mpMissingTexture, mat.ambientTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Ambient);
        }
        if (setTexture(mTextures, material.emissiveTexture.index, mpMissingTexture, mat.emissionTexturePath)) {
            mat.params.hasTexture |= static_cast<uint32_t>(MaterialTextureBit::Emission);
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
