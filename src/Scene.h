#pragma once

#include "Common.h"

#include "Camera.h"
#include "Device.h"
#include "Layout.h"
#include "Sampler.h"
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

        // Device offset are set when uploading to device
        VkDeviceSize deviceVerticesOffset;
        VkDeviceSize deviceIndicesOffset;
    };

    struct Material {
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;

        std::string ambientTexturePath;
        std::string diffuseTexturePath;
        std::string specularTexturePath;
    };

    struct Node {
        std::vector<Mesh> meshes;
        glm::mat4* transform;
        Node* child;
    };

    class Scene
    {
    public:
        MANDRILL_API Scene(std::shared_ptr<Device> pDevice);
        MANDRILL_API ~Scene();

        MANDRILL_API std::shared_ptr<Layout> getLayout(int set) const;

        MANDRILL_API void render(VkCommandBuffer cmd, const std::shared_ptr<Camera> pCamera) const;

        MANDRILL_API Node& addNode(const std::filesystem::path& path, const std::filesystem::path& materialPath = ".");

        /// <summary>
        /// Synchronize buffers to device.
        ///
        /// Upload all buffers from host to device. This function should be called after nodes have been added or
        /// removed from the scene.
        ///
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void update();

        /// <summary>
        /// Set sampler to use for rendering materials.
        /// </summary>
        /// <param name="pSampler">Sampler to be used for all materials</param>
        /// <returns></returns>
        MANDRILL_API void setSampler(const std::shared_ptr<Sampler> pSampler);

    private:
        std::shared_ptr<Device> mpDevice;

        std::vector<Node> mNodes;
        std::vector<Material> mMaterials;
        std::unordered_map<std::string, Texture> mTextures;

        std::shared_ptr<Buffer> mpVertexBuffer;
        std::shared_ptr<Buffer> mpIndexBuffer;
        std::shared_ptr<Buffer> mpUniform;
    };
}; // namespace Mandrill
