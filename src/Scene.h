#pragma once

#include "Common.h"

#include "Device.h"
#include "Texture.h"

namespace Mandrill
{
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoord;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        uint32_t materialIndex;
    };

    struct Material {
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;

        std::shared_ptr<Texture> ambientTexture;
        std::shared_ptr<Texture> diffuseTexture;
        std::shared_ptr<Texture> specularTexture;
    };

    struct Node {
        glm::vec3 translation;
        glm::mat4 rotation;
        glm::vec3 scale;
        std::vector<Mesh> meshes;

        glm::mat4 getTransform()
        {
            return glm::mat4(rotation[0][0] * scale.x, rotation[0][1] * scale.x, rotation[0][2] * scale.x, 0.0f,
                             rotation[1][0] * scale.y, rotation[1][1] * scale.y, rotation[1][2] * scale.y, 0.0f,
                             rotation[2][0] * scale.z, rotation[2][1] * scale.z, rotation[2][2] * scale.z, 0.0f,
                             translation.x, translation.y, translation.z, 1.0f);
        }
    };

    class Scene
    {
    public:
        MANDRILL_API Scene(std::shared_ptr<Device> pDevice);
        MANDRILL_API ~Scene();

        MANDRILL_API void render(VkCommandBuffer cmd) const;

        MANDRILL_API Node& addNode(const std::filesystem::path& path);

    private:
        std::shared_ptr<Device> mpDevice;

        std::vector<Node> mNodes;
        std::vector<Material> mMaterials;
    };
}; // namespace Mandrill
