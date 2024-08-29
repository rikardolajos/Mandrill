#include "Scene.h"

#include "Extension.h "
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
}

Node::~Node()
{
}

void Node::render(VkCommandBuffer cmd, const std::shared_ptr<Camera> pCamera, VkPipelineLayout layout,
                  const std::shared_ptr<const Scene> pScene) const
{
    if (!mVisible) {
        return;
    }

    for (auto meshIndex : mMeshIndices) {
        const Mesh& mesh = pScene->mMeshes[meshIndex];

        // Descriptor for node
        VkDescriptorBufferInfo bufferInfoNode = {
            .buffer = pScene->mpTransforms->getBuffer(),
            .offset = mTransformsOffset,
            .range = sizeof(glm::mat4),
        };

        VkWriteDescriptorSet transformsDescriptor = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfoNode,
        };

        // Descriptor for material params
        VkDescriptorBufferInfo bufferInfoMaterialParams = {
            .buffer = pScene->mpMaterialParams->getBuffer(),
            .offset = pScene->mMaterials[mesh.materialIndex].paramsOffset,
            .range = sizeof(MaterialParams),
        };

        VkWriteDescriptorSet materialParamsDescriptor = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfoMaterialParams,
        };

        // Push descriptors
        std::vector<VkWriteDescriptorSet> descriptors;
        descriptors.push_back(pCamera->getDescriptor(0));
        descriptors.push_back(transformsDescriptor);
        descriptors.push_back(materialParamsDescriptor);

        if (pScene->mMaterials[mesh.materialIndex].params.hasTexture & DIFFUSE_TEXTURE_BIT) {
            descriptors.push_back(
                pScene->mTextures.at(pScene->mMaterials[mesh.materialIndex].diffuseTexturePath).getDescriptor(3));
        } else {
            descriptors.push_back(pScene->mpMissingTexture->getDescriptor(3));
        }

        if (pScene->mMaterials[mesh.materialIndex].params.hasTexture & SPECULAR_TEXTURE_BIT) {
            descriptors.push_back(
                pScene->mTextures.at(pScene->mMaterials[mesh.materialIndex].specularTexturePath).getDescriptor(4));
        } else {
            descriptors.push_back(pScene->mpMissingTexture->getDescriptor(4));
        }

        if (pScene->mMaterials[mesh.materialIndex].params.hasTexture & AMBIENT_TEXTURE_BIT) {
            descriptors.push_back(
                pScene->mTextures.at(pScene->mMaterials[mesh.materialIndex].ambientTexturePath).getDescriptor(5));
        } else {
            descriptors.push_back(pScene->mpMissingTexture->getDescriptor(5));
        }

        if (pScene->mMaterials[mesh.materialIndex].params.hasTexture & EMISSION_TEXTURE_BIT) {
            descriptors.push_back(
                pScene->mTextures.at(pScene->mMaterials[mesh.materialIndex].emissionTexturePath).getDescriptor(6));
        } else {
            descriptors.push_back(pScene->mpMissingTexture->getDescriptor(6));
        }

        if (pScene->mMaterials[mesh.materialIndex].params.hasTexture & NORMAL_TEXTURE_BIT) {
            descriptors.push_back(
                pScene->mTextures.at(pScene->mMaterials[mesh.materialIndex].normalTexturePath).getDescriptor(7));
        } else {
            descriptors.push_back(pScene->mpMissingTexture->getDescriptor(7));
        }

        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, pScene->mDescriptorSet,
                                  static_cast<uint32_t>(descriptors.size()), descriptors.data());

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

Scene::Scene(std::shared_ptr<Device> pDevice) : mpDevice(pDevice)
{
    mpMissingTexture =
        std::make_shared<Texture>(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_SRGB, "missing.png", false);
}

Scene::~Scene()
{
}

std::shared_ptr<Layout> Scene::getLayout(int set)
{
    mDescriptorSet = set;

    std::vector<LayoutDescription> layoutDesc;
    layoutDesc.emplace_back(mDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Camera matrix
    layoutDesc.emplace_back(mDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Model matrix
    layoutDesc.emplace_back(mDescriptorSet, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material colors
    layoutDesc.emplace_back(mDescriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material diffuse texture
    layoutDesc.emplace_back(mDescriptorSet, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material specular texture
    layoutDesc.emplace_back(mDescriptorSet, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material ambient texture
    layoutDesc.emplace_back(mDescriptorSet, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material emission texture
    layoutDesc.emplace_back(mDescriptorSet, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material normal texture

    return std::make_shared<Layout>(mpDevice, layoutDesc, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
}

void Scene::render(VkCommandBuffer cmd, const std::shared_ptr<Camera> pCamera, VkPipelineLayout layout) const
{
    for (auto& node : mNodes) {
        node.render(cmd, pCamera, layout, shared_from_this());
    }
}

Node& Scene::addNode()
{
    Node node = {};

    mNodes.push_back(node);

    return *(mNodes.end() - 1);
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
                Vertex vert;

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

        auto loadTexture = [path, materialPath](std::shared_ptr<Device> pDevice,
                                                std::unordered_map<std::string, Texture>& loadedTextures,
                                                std::string textureFile, std::string& texturePath) -> bool {
            if (textureFile.empty()) {
                return false;
            }

            texturePath =
                std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureFile).string();
            if (loadedTextures.contains(texturePath)) {
                return false;
            }

            loadedTextures.insert(std::make_pair(
                texturePath, Texture(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_UNORM, texturePath, true)));

            return true;
        };

        mat.params.hasTexture = 0;

        if (loadTexture(mpDevice, mTextures, material.diffuse_texname, mat.diffuseTexturePath)) {
            mat.params.hasTexture |= DIFFUSE_TEXTURE_BIT;
        }
        if (loadTexture(mpDevice, mTextures, material.specular_texname, mat.specularTexturePath)) {
            mat.params.hasTexture |= SPECULAR_TEXTURE_BIT;
        }
        if (loadTexture(mpDevice, mTextures, material.ambient_texname, mat.ambientTexturePath)) {
            mat.params.hasTexture |= AMBIENT_TEXTURE_BIT;
        }
        if (loadTexture(mpDevice, mTextures, material.emissive_texname, mat.emissionTexturePath)) {
            mat.params.hasTexture |= EMISSION_TEXTURE_BIT;
        }
        if (loadTexture(mpDevice, mTextures, material.normal_texname, mat.normalTexturePath)) {
            mat.params.hasTexture |= NORMAL_TEXTURE_BIT;
        }

        mMaterials.push_back(mat);
    }

    return newMeshIndices;
}

void Scene::compile()
{
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
    mpVertexBuffer = std::make_shared<Buffer>(mpDevice, verticesSize,
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpIndexBuffer = std::make_shared<Buffer>(mpDevice, indicesSize,
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    mpTransforms = std::make_shared<Buffer>(mpDevice, sizeof(glm::mat4) * mNodes.size(),
                                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    mpMaterialParams =
        std::make_shared<Buffer>(mpDevice, sizeof(MaterialParams) * mMaterials.size(),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    // Associate each node with a part of the transforms buffer
    glm::mat4* transforms = static_cast<glm::mat4*>(mpTransforms->getHostMap());
    for (size_t i = 0; i < mNodes.size(); i++) {
        mNodes[i].mpTransform = transforms + i;
        *mNodes[i].mpTransform = glm::mat4(1.0f);
        mNodes[i].mTransformsOffset = i * sizeof(glm::mat4);
    }

    // Associate each material with a part of the material params buffer
    MaterialParams* materialParams = static_cast<MaterialParams*>(mpMaterialParams->getHostMap());
    for (size_t i = 0; i < mMaterials.size(); i++) {
        mMaterials[i].paramsDevice = materialParams + i;
        *mMaterials[i].paramsDevice = mMaterials[i].params;
        mMaterials[i].paramsOffset = i * sizeof(MaterialParams);
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

void Scene::setSampler(const std::shared_ptr<Sampler> pSampler)
{
    mpMissingTexture->setSampler(pSampler);

    for (auto& texture : mTextures) {
        texture.second.setSampler(pSampler);
    }
}
