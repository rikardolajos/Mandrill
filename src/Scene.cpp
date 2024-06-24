#include "Scene.h"

#include "Log.h"

#include "tiny_obj_loader.h"

using namespace Mandrill;

Scene::Scene(std::shared_ptr<Device> pDevice) : mpDevice(pDevice)
{
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd) const
{
}

Node& Scene::addNode(const std::filesystem::path& path)
{
    Node node;

    Log::info("Loading {}", path.string());

    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = "./";

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

        if (!material.ambient_texname.empty()) {
            mat.ambientTexture = std::make_shared<Texture>(mpDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8_SRGB,
                                                           material.ambient_texname, true);
        }

        if (!material.diffuse_texname.empty()) {
            mat.diffuseTexture = std::make_shared<Texture>(mpDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8_SRGB,
                                                           material.diffuse_texname, true);
        }

        if (!material.specular_texname.empty()) {
            mat.specularTexture = std::make_shared<Texture>(mpDevice, Texture::Type::Texture2D, VK_FORMAT_R8G8B8_SRGB,
                                                            material.specular_texname, true);
        }
        mMaterials.push_back(mat);
    }

    return node;
}
