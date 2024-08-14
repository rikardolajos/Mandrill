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
        MaterialParams* paramsDevice; // This is set during Scene::compile()
        VkDeviceSize paramsOffset;    // This is set during Scene::compile()

        std::string diffuseTexturePath;
        std::string specularTexturePath;
        std::string ambientTexturePath;
        std::string emissionTexturePath;
        std::string normalTexturePath;
    };

    class Scene; // Forward declare scene so Node can befriend it

    class Node
    {
    public:
        MANDRILL_API Node();
        MANDRILL_API ~Node();

        MANDRILL_API void render(VkCommandBuffer cmd, const std::shared_ptr<Pipeline> pPipeline,
                                 const std::shared_ptr<Camera> pCamera,
                                 const std::shared_ptr<const Scene> pScene) const;

        MANDRILL_API void addMesh(uint32_t meshIndex)
        {
            mMeshIndices.push_back(meshIndex);
        }

        MANDRILL_API void setTransform(glm::mat4 transform)
        {
            if (!mpTransform) {
                Log::error("Cannot set transform. Make sure to compile the scene before setting transforms");
                return;
            }
            *mpTransform = transform;
        }

        MANDRILL_API void setVisible(bool visible)
        {
            mVisible = visible;
        }

    private:
        friend Scene;

        std::vector<uint32_t> mMeshIndices;

        glm::mat4* mpTransform;
        VkDeviceSize mTransformsOffset;

        bool mVisible;

        Node* mChild;
    };

    class Scene : public std::enable_shared_from_this<Scene>
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

        MANDRILL_API Node& addNode();

        MANDRILL_API uint32_t addMaterial(Material material);

        MANDRILL_API uint32_t addMesh(const std::vector<Vertex> vertices, const std::vector<uint32_t> indices,
                                   uint32_t materialIndex);

        MANDRILL_API std::vector<uint32_t> addMeshFromFile(const std::filesystem::path& path,
                                                        const std::filesystem::path& materialPath = "");

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
        friend Node;

        std::shared_ptr<Device> mpDevice;

        std::vector<Mesh> mMeshes;
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
