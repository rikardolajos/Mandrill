#pragma once

#include "Common.h"

#include "AccelerationStructure.h"
#include "Camera.h"
#include "Descriptor.h"
#include "Device.h"
#include "Layout.h"
#include "Swapchain.h"
#include "Texture.h"

namespace Mandrill
{
    struct Vertex {
        alignas(16) glm::vec3 position; // Position of vertex in 3D space
        alignas(16) glm::vec3 normal;   // Normal vector of vertex
        alignas(16) glm::vec2 texcoord; // Texture coordinates
        alignas(16) glm::vec3 tangent;  // Tangent vector for normal mapping
        alignas(16) glm::vec3 binormal; // Binormal vector for normal mapping

        /// <summary>
        /// Check if two vertices are exactly equal (so we can remove redundant vertices).
        /// </summary>
        /// <param name="other">Vertex to compare to</param>
        /// <returns>True if equal, otherwise false</returns>
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

    struct alignas(16) MaterialParams {
        glm::vec3 diffuse;
        float shininess;
        glm::vec3 specular;
        float indexOfRefraction;
        glm::vec3 ambient;
        float opacity;
        glm::vec3 emission;
        uint32_t hasTexture;
    };

    struct alignas(16) MaterialDevice {
        MaterialParams params;
        uint32_t diffuseTextureIndex;
        uint32_t specularTextureIndex;
        uint32_t ambientTextureIndex;
        uint32_t emissionTextureIndex;
        uint32_t normalTextureIndex;
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

    struct InstanceData {
        uint32_t verticesOffset; // Offset into global vertex buffer
        uint32_t indicesOffset;  // Offset into global index buffer
    };

    class Scene; // Forward declare scene so Node can befriend it
    class Pipeline;

    /// <summary>
    /// Scene node class for managing a single node in a scene graph. This class can hold meshes and transformations.
    /// </summary>
    class Node
    {
    public:
        /// <summary>
        /// Create a new scene node.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API Node();

        /// <summary>
        /// Destructor for scene node.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Node();

        /// <summary>
        /// Bind the vertex and index buffers of the node's meshes and draw them. Use this instead of Node::render() if
        /// you want more control over which pipeline and descriptors to use.
        /// </summary>
        /// <param name="cmd">Command buffer to use for drawing</param>
        /// <param name="pScene">Scene which the node belongs to</param>
        /// <returns></returns>
        MANDRILL_API void drawMeshes(VkCommandBuffer cmd, const ptr<const Scene> pScene) const;

        /// <summary>
        /// Render a node in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pCamera">Camera that defines which camera matrices to use</param>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        /// <param name="pScene">Scene which the node belongs to</param>
        /// <returns></returns>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<Camera> pCamera, uint32_t frameInFlightIndex,
                                 const ptr<const Scene> pScene) const;

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
        /// Get the TRS transform of the node.
        /// </summary>
        /// <returns>4x4 matrix containing transform</returns>
        MANDRILL_API glm::mat4 getTransform() const
        {
            return mTransform;
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

        /// <summary>
        /// Get the visibility of the node.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API bool getVisible() const
        {
            return mVisible;
        }

        /// <summary>
        /// Get the mesh indices
        /// </summary>
        /// <returns>Vector of mesh indices</returns>
        MANDRILL_API std::vector<uint32_t>& getMeshIndices()
        {
            return mMeshIndices;
        }

    private:
        friend Scene;

        ptr<Pipeline> mpPipeline;

        std::vector<uint32_t> mMeshIndices;

        glm::mat4 mTransform;
        glm::mat4* mpTransformDevice;
        ptr<Descriptor> mpDescriptor;

        bool mVisible;

        std::vector<Node*> mChildren;
    };

    /// <summary>
    /// Scene class that manages a collection of nodes, materials, meshes, and rendering operations.
    /// </summary>
    class Scene : public std::enable_shared_from_this<Scene>
    {
    public:
        MANDRILL_NON_COPYABLE(Scene)

        /// <summary>
        /// Create a new scene.
        /// </summary>
        /// <param name="pDevice">Device to use</param>
        /// <returns></returns>
        MANDRILL_API Scene(ptr<Device> pDevice);

        /// <summary>
        /// Destructor for scene.
        /// </summary>
        /// <returns></returns>
        MANDRILL_API ~Scene();

        /// <summary>
        /// Render all the nodes in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pCamera">Camera that defines which camera matrices to use</param>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        /// <returns></returns>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<Camera> pCamera, uint32_t frameInFlightIndex) const;

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
        /// Add several meshes to a scene by reading them from an OBJ- or GLTF/GLB-file.
        /// </summary>
        /// <param name="path">Path to the file</param>
        /// <param name="materialPath">Path to where the material files are stored (leave to default if the materials
        /// are in the same directory as the OBJ-file, no effect for GLTF/GLB)</param>
        /// <returns>List of mesh indices that can be added to a node in the scene</returns>
        MANDRILL_API std::vector<uint32_t> addMeshFromFile(const std::filesystem::path& path,
                                                           const std::filesystem::path& materialPath = "");

        /// <summary>
        /// Calculate sizes of buffers and allocate resources. Call this after all nodes have been added.
        /// </summary>
        /// <param name="frameInFlightCount">Used to determine how many copies of per-frame resources are
        /// needed</param>
        /// <returns></returns>
        MANDRILL_API void compile(uint32_t frameInFlightCount);

        /// <summary>
        /// Create desciptors for scene.
        ///
        /// The scene will use descriptor sets as follows:
        /// <table>
        /// <caption> Descriptor Layout </caption>
        /// <tr><th> Usage <th> Set <th> Binding <th> Type <th>
        /// <tr><td> Camera matrix (struct CameraMatrices) <td> 0 <td> 0 <td> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        /// <tr><td> Model matrix (mat4) <td> 1 <td> 0 <td> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        /// <tr><td> Material params (struct MaterialParams) <td> 2 <td> 0 <td> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        /// <tr><td> Material diffuse texture <td> 2 <td> 1 <td> VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
        /// <tr><td> Material specular texture <td> 2 <td> 2 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// <tr><td> Material ambient texture <td> 2 <td> 3 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// <tr><td> Material emission texture <td> 2 <td> 4 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// <tr><td> Material normal texture <td> 2 <td> 5 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// <tr><td> Environment map texture <td> 3 <td> 0 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// </table>
        /// </summary>
        /// <param name="descriptorSetLayouts">Vector with descriptor set layouts to use (get from
        /// Shader::getDescriptorSetLayouts())</param>
        /// <param name="frameInFlightCount">Used to determine how many copies of per-frame resources are
        /// needed</param>
        /// <returns></returns>
        MANDRILL_API void createDescriptors(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                            uint32_t frameInFlightCount);

        /// <summary>
        /// Create ray-tracing desciptors for scene.
        ///
        /// The scene will use descriptor sets as follows:
        /// <table>
        /// <caption> Ray-tracing Descriptor Layout </caption>
        /// <tr><th> Usage <th> Set <th> Binding <th> Type <th>
        /// <tr><td> Camera matrix (struct CameraMatrices) <td> 0 <td> 0 <td> VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        /// <tr><td> Acceleration structure <td> 1 <td> 0 <td> VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        /// <tr><td> Global vertex buffer <td> 1 <td> 1 <td> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        /// <tr><td> Global index buffer <td> 1 <td> 2 <td> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        /// <tr><td> Instance data (vertex and index offsets) <td> 1 <td> 3 <td> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        /// <tr><td> Global material buffer <td> 1 <td> 4 <td> VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        /// <tr><td> Global texture array <td> 1 <td> 5 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER (Array)
        /// <tr><td> Environment map texture <td> 2 <td> 0 <td> VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        /// </table>
        /// </summary>
        /// <param name="descriptorSetLayouts">Vector with descriptor set layouts to use (get from
        /// Shader::getDescriptorSetLayouts())</param> <param name="pAccelerationStructure">Acceleration structure to
        /// bind</param> <param name="frameInFlightCount">Used to determine how many copies of per-frame resources are
        /// needed</param>
        /// <returns></returns>
        MANDRILL_API void createRayTracingDescriptors(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
                                                      const ptr<AccelerationStructure> pAccelerationStructure,
                                                      uint32_t frameInFlightCount);

        /// <summary>
        /// Synchronize buffers to device.
        ///
        /// Upload all buffers from host to device. This function should be called after updates have been made to
        /// the scene.
        ///
        /// Node transforms and material parameters are kept host coherent and can be changed without requiring a
        /// new sync.
        ///
        /// </summary>
        /// <returns></returns>
        MANDRILL_API void syncToDevice();

        /// <summary>
        /// Bind ray-tracing descriptors.
        /// </summary>
        /// <param name="cmd">Command buffer to use</param>
        /// <param name="pCamera">Camera to use</param>
        /// <param name="layout">Pipeline layout</param>
        /// <param name="frameInFlightIndex">Used to determine which resource to use</param>
        /// <returns></returns>
        MANDRILL_API void bindRayTracingDescriptors(VkCommandBuffer cmd, ptr<Camera> pCamera, VkPipelineLayout layout,
                                                    uint32_t frameInFlightIndex);

        /// <summary>
        /// Get the list of all nodes in the scene.
        /// </summary>
        /// <returns>Vector of nodes</returns>
        MANDRILL_API std::vector<Node>& getNodes()
        {
            return mNodes;
        }

        /// <summary>
        /// Get all textures in the scene.
        /// </summary>
        /// <returns>Unordered map of textures</returns>
        MANDRILL_API std::unordered_map<std::string, ptr<Texture>>& getTextures()
        {
            return mTextures;
        }

        /// <summary>
        /// Get the number of vertices in the scene.
        /// </summary>
        /// <returns>Number of vertices</returns>
        MANDRILL_API uint32_t getVertexCount() const
        {
            return mVertexCount;
        }

        /// <summary>
        /// Get the number of indices in the scene.
        /// </summary>
        /// <returns>Number of indices</returns>
        MANDRILL_API uint32_t getIndexCount() const
        {
            return mIndexCount;
        }

        /// <summary>
        /// Get the number of meshes in the scene.
        /// </summary>
        /// <returns>Number of meshes</returns>
        MANDRILL_API uint32_t getMeshCount()
        {
            return count(mMeshes);
        }

        /// <summary>
        /// Get the number of materials in the scene.
        /// </summary>
        /// <returns>Number of materials</returns>
        MANDRILL_API uint32_t getMaterialCount() const
        {
            return count(mMaterials);
        }

        /// <summary>
        /// Get the number of textures in the scene.
        /// </summary>
        /// <returns>Number of textures</returns>
        MANDRILL_API uint32_t getTextureCount() const
        {
            return count(mTextures);
        }

        /// <summary>
        /// Get the number of vertices in a mesh.
        /// </summary>
        /// <param name="meshIndex">Index of mesh to get the vertices from</param>
        /// <returns>Number of vertices</returns>
        MANDRILL_API uint32_t getMeshVertexCount(uint32_t meshIndex) const
        {
            return count(mMeshes[meshIndex].vertices);
        }

        /// <summary>
        /// Get the number of indices in a mesh.
        /// </summary>
        /// <param name="meshIndex">Index of mesh to get the vertices from</param>
        /// <returns>Number of indices</returns>
        MANDRILL_API uint32_t getMeshIndexCount(uint32_t meshIndex) const
        {
            return count(mMeshes[meshIndex].indices);
        }

        /// <summary>
        /// Get the address of a mesh's vertices buffer.
        /// </summary>
        /// <param name="meshIndex">Index of mesh to look up</param>
        /// <returns>Device address</returns>
        MANDRILL_API VkDeviceAddress getMeshVertexAddress(uint32_t meshIndex) const
        {
            return mpVertexBuffer->getDeviceAddress() + mMeshes[meshIndex].deviceVerticesOffset;
        }

        /// <summary>
        /// Get the address of a mesh's indices buffer.
        /// </summary>
        /// <param name="meshIndex">Index of the mesh to look up</param>
        /// <returns>Device address</returns>
        MANDRILL_API VkDeviceAddress getMeshIndexAddress(uint32_t meshIndex) const
        {
            return mpIndexBuffer->getDeviceAddress() + mMeshes[meshIndex].deviceIndicesOffset;
        }

        /// <summary>
        /// Get the matieral index of a mesh.
        /// </summary>
        /// <param name="meshIndex">Index of the mesh to look up</param>
        /// <returns></returns>
        MANDRILL_API uint32_t getMeshMaterialIndex(uint32_t meshIndex) const
        {
            return mMeshes[meshIndex].materialIndex;
        }

        /// <summary>
        /// Set an environment map for the scene.
        /// </summary>
        /// <param name="pTexture">Texture to use as environment map</param>
        /// <returns></returns>
        MANDRILL_API void setEnvironmentMap(ptr<Texture> pTexture)
        {
            mpEnvironmentMap = pTexture;
        }

    private:
        friend Node;

        std::vector<uint32_t> loadFromOBJ(const std::filesystem::path& path,
                                          const std::filesystem::path& materialPath = "");
        std::vector<uint32_t> loadFromGLTF(const std::filesystem::path& path,
                                           const std::filesystem::path& materialPath = "");
        void addTexture(std::string texturePath);
        void addTextureFromMemory(const uint8_t* pData, size_t size, const std::string& textureName);

        ptr<Device> mpDevice;

        std::vector<Mesh> mMeshes;
        std::vector<Node> mNodes;
        std::vector<Material> mMaterials;
        std::unordered_map<std::string, ptr<Texture>> mTextures;
        ptr<Texture> mpEnvironmentMap;
        ptr<Descriptor> mpEnvironmentMapDescriptor;

        ptr<Buffer> mpVertexBuffer;
        ptr<Buffer> mpIndexBuffer;
        ptr<Buffer> mpTransforms;
        ptr<Buffer> mpMaterialParams;

        ptr<Texture> mpMissingTexture;

        ptr<Descriptor> mpRayTracingDescriptor;
        ptr<Buffer> mpMaterialBuffer; // Almost same as mpMaterialParams but for ray tracing
        ptr<Buffer> mpInstanceDataBuffer;

        uint32_t mVertexCount;
        uint32_t mIndexCount;
    };
}; // namespace Mandrill
