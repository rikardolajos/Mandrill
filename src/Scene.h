#pragma once

#include "Common.h"

#include "Camera.h"
#include "Descriptor.h"
#include "Device.h"
#include "Layout.h"
#include "Pipeline.h"
#include "Sampler.h"
#include "Swapchain.h"
#include "Texture.h"

namespace Mandrill
{
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texcoord;
        glm::vec3 tangent;
        glm::vec3 binormal;

        bool operator==(const Vertex& other) const
        {
            return position == other.position && normal == other.normal && texcoord == other.texcoord &&
                   tangent == other.tangent && binormal == other.binormal;
        }
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        uint32_t materialIndex{};

        // Device offset are set when uploading to device
        VkDeviceSize deviceVerticesOffset{};
        VkDeviceSize deviceIndicesOffset{};
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
        MaterialParams params{};
        MaterialParams* paramsDevice{}; // This is set during Scene::compile()
        VkDeviceSize paramsOffset{};    // This is set during Scene::compile()

        std::string diffuseTexturePath;
        std::string specularTexturePath;
        std::string ambientTexturePath;
        std::string emissionTexturePath;
        std::string normalTexturePath;

        ptr<Descriptor> pDescriptor;
    };

    class Scene; // Forward declare scene so Node can befriend it

    class Node
    {
    public:
        MANDRILL_API Node();
        MANDRILL_API ~Node();

        /// <summary>
        /// Render a node in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pCamera">Camera that defines which camera matrices to use</param>
        /// <param name="pScene">Scene which the node belongs to</param>
        /// <returns></returns>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<Camera> pCamera, const ptr<const Scene> pScene) const;

        /// <summary>
        /// Add a mesh to the node.
        /// </summary>
        /// <param name="meshIndex">Mesh index that was received during mesh creation</param>
        /// <returns></returns>
        MANDRILL_API void addMesh(uint32_t meshIndex)
        {
            mMeshIndices.push_back(meshIndex);
        }

        /// <summary>
        /// Set pipeline to use when rendering node
        /// </summary>
        /// <param name="pPipeline">Pipeline to use</param>
        /// <returns></returns>
        MANDRILL_API void setPipeline(ptr<Pipeline> pPipeline)
        {
            mpPipeline = pPipeline;
        }

        /// <summary>
        /// Set the TRS transform of the node.
        /// </summary>
        /// <param name="transform">Transform to use</param>
        /// <returns></returns>
        MANDRILL_API void setTransform(glm::mat4 transform)
        {
            mTransform = transform;
        }

        /// <summary>
        /// Set weather the node should be rendered or not.
        /// </summary>
        /// <param name="visible">True to render the node, otherwise false</param>
        /// <returns></returns>
        MANDRILL_API void setVisible(bool visible)
        {
            mVisible = visible;
        }

    private:
        friend Scene;

        ptr<Pipeline> mpPipeline;

        std::vector<uint32_t> mMeshIndices;

        glm::mat4 mTransform;
        glm::mat4* mpTransformDevice;
        ptr<Descriptor> pDescriptor;

        bool mVisible;

        std::vector<Node*> mChildren;
    };

    class Scene : public std::enable_shared_from_this<Scene>
    {
    public:
        /// <summary>
        /// Create a new scene.
        ///
        /// The scene will use descriptor sets as follows:
        ///     Camera matrix (struct CameraMatrices): Set = 0, Binding = 0, Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        ///     Model matrix (mat4): Set = 1, Binding = 0, Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        ///     Material params (struct MaterialParams): Set = 2, Binding = 0, Type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        ///     Material diffuse texture: Set = 2, Binding = 1, Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        ///     Material specular texture: Set = 2, Binding = 2, Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        ///     Material ambient texture: Set = 2, Binding = 3, Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        ///     Material emission texture: Set = 2, Binding = 4, Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        ///     Material normal texture: Set = 2, Binding = 5, Type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        ///
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <param name="pSwapchain">Swapchain is used to determine how many descriptors will be used</param>
        /// <returns></returns>
        MANDRILL_API Scene(ptr<Device> pDevice, ptr<Swapchain> pSwapchain);
        MANDRILL_API ~Scene();

        /// <summary>
        /// Render all the nodes in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pCamera">Camera that defines which camera matrices to use</param>
        /// <returns></returns>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<Camera> pCamera) const;

        /// <summary>
        /// Add a node to the scene.
        /// </summary>
        /// <returns>A ptr to the created node</returns>
        MANDRILL_API ptr<Node> addNode();

        /// <summary>
        /// Add a material to the scene.
        /// </summary>
        /// <param name="material">Material struct defining the new material</param>
        /// <returns>Material index that can be used to create new meshes</returns>
        MANDRILL_API uint32_t addMaterial(Material material);

        /// <summary>
        /// Add a mesh to the scene.
        /// </summary>
        /// <param name="vertices">List of vertices that make up the mesh</param>
        /// <param name="indices">List of indices that describes how the vertices are connected</param>
        /// <param name="materialIndex">Which material should be used for the mesh</param>
        /// <returns>Mesh index that can be added to a node in the scene</returns>
        MANDRILL_API uint32_t addMesh(const std::vector<Vertex> vertices, const std::vector<uint32_t> indices,
                                      uint32_t materialIndex);

        /// <summary>
        /// Add several meshes to a scene by reading them from an OBJ-file.
        /// </summary>
        /// <param name="path">Path to the OBJ-file</param>
        /// <param name="materialPath">Path to where the material files are stored (leave to default if the materials
        /// are in the same directory as the OBJ-file)</param> <returns>List of mesh indices that can be added to a node
        /// in the scene</returns>
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
        /// Node transforms and material parameters are kept host coherent and can be changed without requiring a new
        /// sync.
        ///
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void syncToDevice();

        /// <summary>
        /// Set sampler to use for rendering materials.
        /// </summary>
        /// <param name="pSampler">Sampler to be used for all materials</param>
        /// <returns></returns>
        MANDRILL_API void setSampler(const ptr<Sampler> pSampler);

        /// <summary>
        /// Get the layout used by the scene.
        /// </summary>
        /// <returns>Pointer to layout</returns>
        MANDRILL_API ptr<Layout> getLayout();

        /// <summary>
        /// Get the list of all nodes in the scene
        /// </summary>
        /// <returns>Vector of nodes</returns>
        MANDRILL_API std::vector<Node>& getNodes()
        {
            return mNodes;
        }

    private:
        friend Node;

        void addTexture(std::string texturePath);
        void createDescriptors();

        ptr<Device> mpDevice;
        ptr<Swapchain> mpSwapchain;

        std::vector<Mesh> mMeshes;
        std::vector<Node> mNodes;
        std::vector<Material> mMaterials;
        std::unordered_map<std::string, ptr<Texture>> mTextures;

        ptr<Buffer> mpVertexBuffer;
        ptr<Buffer> mpIndexBuffer;
        ptr<Buffer> mpTransforms;
        ptr<Buffer> mpMaterialParams;

        ptr<Texture> mpMissingTexture;
    };
}; // namespace Mandrill
