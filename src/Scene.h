#pragma once

#include "Common.h"

#include "Camera.h"
#include "Device.h"
#include "Layout.h"
#include "Pipelines/Pipeline.h"
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

    struct MaterialParams {
        glm::vec3 diffuse;
        float shininess;
        glm::vec3 specular;
        float indexOfRefraction;
        glm::vec3 ambient;
        float opacity;
        glm::vec3 emission;
        uint32_t hasTexture;
    };

    struct Material {
        MaterialParams params;
        MaterialParams* paramsDevice;
        VkDeviceSize paramsOffset;

        std::string diffuseTexturePath;
        std::string specularTexturePath;
        std::string ambientTexturePath;
        std::string emissionTexturePath;
        std::string normalTexturePath;
    };

    struct Node {
        std::vector<Mesh> meshes;
        glm::mat4* transform;
        VkDeviceSize transformsOffset;
        Node* child;
    };

    class Scene
    {
    public:
        MANDRILL_API Scene(std::shared_ptr<Device> pDevice);
        MANDRILL_API ~Scene();

        /// <summary>
        /// Get the layout that the scene uses.
        ///
        /// Use this layout to create the pipeline that will render the scene.
        /// </summary>
        /// <param name="set">Which descriptor set to use. This value will be stored internally and reused during
        /// render().</param>
        /// <returns>Layout used by the scene</returns>
        MANDRILL_API std::shared_ptr<Layout> getLayout(int set);

        MANDRILL_API void render(VkCommandBuffer cmd, const std::shared_ptr<Pipeline> pPipeline,
                                 const std::shared_ptr<Camera> pCamera) const;

        MANDRILL_API Node& addNode(const std::filesystem::path& path, const std::filesystem::path& materialPath = "");

        /// <summary>
        /// Calculate sizes of buffers and allocate resources. Call this after all nodes have been added.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void compile();

        /// <summary>
        /// Synchronize buffers to device.
        ///
        /// Upload all buffers from host to device. This function should be called after updates have been done to
        /// the scene.
        ///
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void syncToDevice();

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
        std::shared_ptr<Buffer> mpTransforms;
        std::shared_ptr<Buffer> mpMaterialParams;

        uint32_t mDescriptorSet = 0;

        std::shared_ptr<Texture> mpMissingTexture;
    };
}; // namespace Mandrill
