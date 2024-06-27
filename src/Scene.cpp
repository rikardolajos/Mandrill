#include "Scene.h"

#include "Log.h"

#include "tiny_obj_loader.h"

using namespace Mandrill;

Scene::Scene(std::shared_ptr<Device> pDevice) : mpDevice(pDevice)
{
    // vkGetDescriptorSetLayoutSizeEXT(mpDevice->getDevice(), );
}

Scene::~Scene()
{
    // Deallocate VkDescriptorPool for acceleration structure?
}

std::shared_ptr<Layout> Scene::getLayout(int set) const
{
    if (set == 0) {
        Log::error("Set 0 is reserved for the acceleration structure");
    }

    std::vector<LayoutDescription> layoutDesc;
    layoutDesc.emplace_back(0, 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                            VK_SHADER_STAGE_RAYGEN_BIT_KHR); // Acceleration structure
    layoutDesc.emplace_back(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Camera matrix
    layoutDesc.emplace_back(set, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Model matrix
    layoutDesc.emplace_back(set, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material colors
    layoutDesc.emplace_back(set, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            VK_SHADER_STAGE_ALL_GRAPHICS); // Material textures

    return std::make_shared<Layout>(mpDevice, layoutDesc, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);
}

void Scene::render(VkCommandBuffer cmd, const std::shared_ptr<Camera> pCamera) const
{
    for (auto& node : mNodes) {
        for (auto& mesh : node.meshes) {
            //// Push descriptors
            // std::array<VkWriteDescriptorSet, 3> descriptors;
            // descriptors[0] = pCamera->getDescriptor(0);
            // descriptors[1] = mMaterials[mesh.materialIndex].diffuseTexture->getDescriptor(1);
            // descriptors[2] = node.getTransform();
            // vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mpPipeline->getLayout(), 0,
            //                           static_cast<uint32_t>(descriptors.size()), descriptors.data());

            //// Bind vertex and index buffers
            // std::array<VkBuffer, 1> vertexBuffers = {mpVertexBuffer->getBuffer()};
            // std::array<VkDeviceSize, 1> offsets = {0};
            // vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(vertexBuffers.size()), vertexBuffers.data(),
            //                        offsets.data());
            // vkCmdBindIndexBuffer(cmd, mpIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

            //// Draw mesh
            // vkCmdDrawIndexed(cmd, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);
        }
    }
}

Node& Scene::addNode(const std::filesystem::path& path, const std::filesystem::path& materialPath)
{
    Node node = {
        //.transform = glm::mat4(1.0f),
    };

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
        return node;
    }

    if (!reader.Warning().empty()) {
        Log::warning("TinyObjReader: {}", reader.Warning());
    }

    auto& attrib = reader.GetAttrib();
    auto& shapes = reader.GetShapes();
    auto& materials = reader.GetMaterials();

    // Loop over shapes
    for (auto& shape : shapes) {
        // Loop over faces
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            Mesh mesh;
            size_t fv = size_t(shape.mesh.num_face_vertices[f]);

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

                mesh.vertices.push_back(vert);
                mesh.indices.push_back(idx.vertex_index);
            }
            indexOffset += fv;

            // Per-face material
            mesh.materialIndex = shape.mesh.material_ids[f];
            node.meshes.push_back(mesh);
        }
    }

    mNodes.push_back(node);

    // Load materials
    for (auto& material : materials) {
        Material mat;
        mat.ambient.r = material.ambient[0];
        mat.ambient.g = material.ambient[1];
        mat.ambient.b = material.ambient[2];

        mat.diffuse.r = material.diffuse[0];
        mat.diffuse.g = material.diffuse[1];
        mat.diffuse.b = material.diffuse[2];

        mat.specular.r = material.specular[0];
        mat.specular.g = material.specular[1];
        mat.specular.b = material.specular[2];

        auto loadTexture = [path, materialPath](std::shared_ptr<Device> pDevice,
                                                std::unordered_map<std::string, Texture> loadedTextures,
                                                std::string textureFile, std::string& texturePath) {
            if (textureFile.empty()) {
                return;
            }

            texturePath =
                std::filesystem::canonical(path.parent_path() / materialPath.relative_path() / textureFile).string();
            if (loadedTextures.contains(texturePath)) {
                return;
            }

            loadedTextures.insert(std::make_pair(
                texturePath, Texture(pDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8A8_SRGB, texturePath, true)));
        };

        loadTexture(mpDevice, mTextures, material.ambient_texname, mat.ambientTexturePath);
        loadTexture(mpDevice, mTextures, material.diffuse_texname, mat.diffuseTexturePath);
        loadTexture(mpDevice, mTextures, material.specular_texname, mat.specularTexturePath);

        mMaterials.push_back(mat);
    }

    return node;
}

void Scene::update()
{
    // Calculate size of buffers
    size_t verticesSize = 0;
    size_t indicesSize = 0;
    for (auto& n : mNodes) {
        for (auto& m : n.meshes) {
            verticesSize += m.vertices.size() * sizeof(m.vertices[0]);
            indicesSize += m.indices.size() * sizeof(m.indices[0]);
        }
    }

    // Allocate device buffers
    mpVertexBuffer = std::make_shared<Buffer>(mpDevice, verticesSize,
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mpIndexBuffer = std::make_shared<Buffer>(mpDevice, indicesSize,
                                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Copy from host to device
    VkDeviceSize verticesOffset = 0;
    VkDeviceSize indicesOffset = 0;
    for (auto& node : mNodes) {
        for (auto& mesh : node.meshes) {
            size_t vertSize = mesh.vertices.size() * sizeof(mesh.vertices[0]);
            size_t indxSize = mesh.indices.size() * sizeof(mesh.indices[0]);

            mpVertexBuffer->copyFromHost(mesh.vertices.data(), vertSize, verticesOffset);
            mpIndexBuffer->copyFromHost(mesh.indices.data(), indxSize, indicesOffset);

            mesh.deviceVerticesOffset = verticesOffset;
            mesh.deviceIndicesOffset = indicesOffset;

            verticesOffset += vertSize;
            indicesOffset += indxSize;
        }
    }
}

void Scene::setSampler(const std::shared_ptr<Sampler> pSampler)
{
    for (auto& texture : mTextures) {
        texture.second.setSampler(pSampler);
    }
}
