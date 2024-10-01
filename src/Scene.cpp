#include "Scene.h"

#include "Extension.h "
#include "Helpers.h"
#include "Log.h"

#include "tiny_obj_loader.h"

using namespace Mandrill;

enum MaterialTextureBit {
    DIFFUSE_TEXTURE_BIT = 1 << 0,
    SPECULAR_TEXTURE_BIT = 1 << 1,
    AMBIENT_TEXTURE_BIT = 1 << 2,
    EMISSION_TEXTURE_BIT = 1 << 3,
    NORMAL_TEXTURE_BIT = 1 << 4,
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

void Node::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, VkPipelineLayout layout,
                  const ptr<const Scene> pScene) const
{
    if (!mVisible) {
        return;
    }

    std::memcpy(mpTransformDevice + pScene->mpSwapchain->getInFlightIndex(), &mTransform, sizeof(glm::mat4));

    // Bind descriptor set for node
    std::array<VkDescriptorSet, 1> descriptorSetNode = {};
    descriptorSetNode[0] = pDescriptor->getSet(pScene->mpSwapchain->getInFlightIndex());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1,
                            static_cast<uint32_t>(descriptorSetNode.size()), descriptorSetNode.data(), 0, nullptr);

    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];

        // Bind descriptor set for material
        std::array<VkDescriptorSet, 1> descriptorSetMaterial = {};
        descriptorSetMaterial[0] = pScene->mMaterials[mesh.materialIndex].pDescriptor->getSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2,
                                static_cast<uint32_t>(descriptorSetMaterial.size()), descriptorSetMaterial.data(), 0,
                                nullptr);

        // Bind vertex and index buffers
        std::array<VkBuffer, 1> vertexBuffers = {pScene->mpVertexBuffer->getBuffer()};
        std::array<VkDeviceSize, 1> offsets = {mesh.deviceVerticesOffset};
        vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(),
                               offsets.data());
        vkCmdBindIndexBuffer(cmd, pScene->mpIndexBuffer->getBuffer(), mesh.deviceIndicesOffset, VK_INDEX_TYPE_UINT32);

        // Draw mesh
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
    }
}

Scene::Scene(ptr<Device> pDevice, ptr<Swapchain> pSwapchain) : mpDevice(pDevice), mpSwapchain(pSwapchain)
{
    mpMissingTexture =
        make_ptr<Texture>(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_SRGB, "missing.png", false);
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd, const ptr<Camera> pCamera, VkPipelineLayout layout) const
{
    // Bind descriptor set for camera matrices
    std::array<VkDescriptorSet, 1> descriptors = {};
    descriptors[0] = pCamera->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, static_cast<uint32_t>(descriptors.size()),
                            descriptors.data(), 0, nullptr);

    for (auto& node : mNodes) {
        node.render(cmd, pCamera, layout, shared_from_this());
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
    mMaterials.push_back(material);

    return static_cast<uint32_t>(mMaterials.size() - 1);
}

uint32_t Scene::addMesh(const std::vector<Vertex> vertices, const std::vector<uint32_t> indices, uint32_t materialIndex)
{
    Mesh mesh = {
        .vertices = vertices,
        .indices = indices,
        .materialIndex = materialIndex,
    };

    mMeshes.push_back(mesh);

    return static_cast<uint32_t>(mMeshes.size() - 1);
}

std::vector<uint32_t> Scene::addMeshFromFile(const std::filesystem::path& path,
                                             const std::filesystem::path& materialPath)
{
    std::vector<uint32_t> newMeshIndices;

    Log::info("Loading {}", path.string());

    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = materialPath.string();
    readerConfig.triangulate = true;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path.string(), readerConfig)) {
        if (!reader.Error().empty()) {
            Log::error("TinyObjReader: {}", reader.Error());
        }
        Log::error("Failed to load {}", path.string());
    }

    if (!reader.Warning().empty()) {
        Log::warning("TinyObjReader: {}", reader.Warning());
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
                shapeMesh[meshIndex].materialIndex = materialIndex;
                indices[meshIndex] += 1;
            }

            indexOffset += fv;
        }

        for (auto& mesh : shapeMesh) {
            mMeshes.push_back(mesh);
            newMeshIndices.push_back(static_cast<uint32_t>(mMeshes.size() - 1));
        }
    }

    // Calculate tangent space for each face (triangle)
    for (uint32_t i = 0; i < newMeshIndices.size(); i++) {
        Mesh& mesh = mMeshes[newMeshIndices.at(i)];
        for (uint32_t j = 0; j < mesh.indices.size(); j += 3) {
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

        auto loadTexture = [path, materialPath](ptr<Device> pDevice,
                                                std::unordered_map<std::string, ptr<Texture>>& loadedTextures,
                                                std::string textureFile, std::string& texturePath) -> bool {
            if (textureFile.empty()) {
                return false;
            }

            texturePath =
                std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureFile).string();
            if (loadedTextures.contains(texturePath)) {
                return false;
            }

            auto pTexture =
                make_ptr<Texture>(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, texturePath, true);
            loadedTextures.insert(std::make_pair(texturePath, pTexture));

            return true;
        };

        auto setMissingTexture = [](std::unordered_map<std::string, ptr<Texture>>& loadedTextures,
                                    const std::string& texturePath, ptr<Texture> pMissingTexture) {
            loadedTextures.insert(std::make_pair(texturePath, pMissingTexture));
        };

        mat.params.hasTexture = 0;

        if (loadTexture(mpDevice, mTextures, material.diffuse_texname, mat.diffuseTexturePath)) {
            mat.params.hasTexture |= DIFFUSE_TEXTURE_BIT;
        } else {
            setMissingTexture(mTextures, material.diffuse_texname, mpMissingTexture);
        }
        if (loadTexture(mpDevice, mTextures, material.specular_texname, mat.specularTexturePath)) {
            mat.params.hasTexture |= SPECULAR_TEXTURE_BIT;
        } else {
            setMissingTexture(mTextures, material.specular_texname, mpMissingTexture);
        }
        if (loadTexture(mpDevice, mTextures, material.ambient_texname, mat.ambientTexturePath)) {
            mat.params.hasTexture |= AMBIENT_TEXTURE_BIT;
        } else {
            setMissingTexture(mTextures, material.ambient_texname, mpMissingTexture);
        }
        if (loadTexture(mpDevice, mTextures, material.emissive_texname, mat.emissionTexturePath)) {
            mat.params.hasTexture |= EMISSION_TEXTURE_BIT;
        } else {
            setMissingTexture(mTextures, material.emissive_texname, mpMissingTexture);
        }
        if (loadTexture(mpDevice, mTextures, material.normal_texname, mat.normalTexturePath)) {
            mat.params.hasTexture |= NORMAL_TEXTURE_BIT;
        } else {
            setMissingTexture(mTextures, material.normal_texname, mpMissingTexture);
        }

        mMaterials.push_back(mat);
    }

    return newMeshIndices;
}

void Scene::compile()
{
    if (mpMissingTexture->getSampler() == VK_NULL_HANDLE) {
        Log::error("Scene: Sampler must be set before calling compile()");
    }

    // Calculate size of buffers
    size_t verticesSize = 0;
    size_t indicesSize = 0;
    for (auto& node : mNodes) {
        for (auto meshIndex : node.mMeshIndices) {
            auto& mesh = mMeshes[meshIndex];
            verticesSize += mesh.vertices.size() * sizeof(mesh.vertices[0]);
            indicesSize += mesh.indices.size() * sizeof(mesh.indices[0]);
        }
    }

    // Allocate device buffers
    mpVertexBuffer =
        make_ptr<Buffer>(mpDevice, verticesSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpIndexBuffer =
        make_ptr<Buffer>(mpDevice, indicesSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceSize alignment = mpDevice->getProperties().physicalDevice.limits.minUniformBufferOffsetAlignment;
    uint32_t copies = mpSwapchain->getFramesInFlightCount();

    // Transforms can change between frames, material parameters can not
    VkDeviceSize transformsSize = Helpers::alignTo(sizeof(glm::mat4) * mNodes.size() * copies, alignment);
    mpTransforms = make_ptr<Buffer>(mpDevice, transformsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize materialParamsSize = Helpers::alignTo(sizeof(MaterialParams) * mMaterials.size(), alignment);
    mpMaterialParams = make_ptr<Buffer>(mpDevice, materialParamsSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Associate each node with a part of the transforms buffer, and with multiple copies for each frame in flight
    glm::mat4* transforms = static_cast<glm::mat4*>(mpTransforms->getHostMap());
    for (size_t i = 0; i < mNodes.size(); i++) {
        mNodes[i].mpTransformDevice = transforms + i * copies;
        for (size_t c = 0; c < copies; c++) {
            *(mNodes[i].mpTransformDevice + c) = glm::mat4(1.0f);
        }
    }

    // Associate each material with a part of the material params buffer
    MaterialParams* materialParams = static_cast<MaterialParams*>(mpMaterialParams->getHostMap());
    for (size_t i = 0; i < mMaterials.size(); i++) {
        mMaterials[i].paramsDevice = materialParams + i;
        *mMaterials[i].paramsDevice = mMaterials[i].params;
        mMaterials[i].paramsOffset = Helpers::alignTo(i * sizeof(MaterialParams), alignment);
    }

    createDescriptors();
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

            size_t vertSize = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            size_t indxSize = mesh.indices.size() * sizeof(mesh.indices[0]);

            vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());

            mesh.deviceVerticesOffset = verticesOffset;
            mesh.deviceIndicesOffset = indicesOffset;

            verticesOffset += vertSize;
            indicesOffset += indxSize;
        }
    }

    mpVertexBuffer->copyFromHost(vertices.data(), vertices.size() * sizeof(vertices[0]), 0);
    mpIndexBuffer->copyFromHost(indices.data(), indices.size() * sizeof(indices[0]), 0);
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
    desc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Camera matrices
    desc.emplace_back(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Model matrix
    desc.emplace_back(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material parameters
    desc.emplace_back(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material diffuse texture
    desc.emplace_back(2, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material specular texture
    desc.emplace_back(2, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material ambient texture
    desc.emplace_back(2, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material emission texture
    desc.emplace_back(2, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      VK_SHADER_STAGE_ALL_GRAPHICS); // Material normal texture

    return make_ptr<Layout>(mpDevice, desc);
}


void Scene::createDescriptors()
{
    ptr<Layout> pLayout = getLayout();

    // Node transform
    for (auto& node : mNodes) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mpTransforms);

        // Set layout for set 1
        auto layout = pLayout->getDescriptorSetLayouts()[1];
        node.pDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout, mpSwapchain->getFramesInFlightCount());
    }

    // Materials
    for (auto& mat : mMaterials) {
        std::vector<DescriptorDesc> desc;
        desc.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, mpMaterialParams);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.diffuseTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.specularTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.ambientTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.emissionTexturePath]);
        desc.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, mTextures[mat.normalTexturePath]);

        // Set layout for set 2
        auto layout = pLayout->getDescriptorSetLayouts()[2];
        mat.pDescriptor = std::make_unique<Descriptor>(mpDevice, desc, layout);
    }
}
