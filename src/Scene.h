#pragma once

#include "Common.h"

#include "AABB.h"
#include "AccelerationStructure.h"
#include "Camera.h"
#include "Descriptor.h"
#include "Device.h"
#include "DynamicBuffer.h"
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

        AABB boundingBox{};
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
    };

    struct InstanceData {
        uint32_t verticesOffset; // Offset into global vertex buffer
        uint32_t indicesOffset;  // Offset into global index buffer
    };

    class Scene; // Forward declare scene so Node can befriend it
    class Pipeline;
    class Shader;

    /// <summary>
    /// Scene node class for managing a single node in a scene graph. This class can hold meshes and transformations.
    /// </summary>
    class Node
    {
    public:
        /// <summary>
        /// Create a new scene node.
        /// </summary>
        MANDRILL_API Node();

        /// <summary>
        /// Destructor for scene node.
        /// </summary>
        MANDRILL_API ~Node();

        /// <summary>
        /// Bind the vertex and index buffers of the node's meshes and draw them. Use this instead of Node::render() if
        /// you want more control over which pipeline and descriptors to use.
        /// </summary>
        /// <param name="cmd">Command buffer to use for drawing</param>
        /// <param name="pScene">Scene which the node belongs to</param>
        MANDRILL_API void drawMeshes(VkCommandBuffer cmd, const ptr<const Scene> pScene) const;

        /// <summary>
        /// Render a node in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pScene">Scene which the node belongs to</param>
        /// <param name="frameInFlightIndex">Which copy of the per-frame resources to use, the current frame by
        /// default</param>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<const Scene> pScene,
                                 uint32_t frameInFlightIndex = kCurrentFrameInFlight) const;

        /// <summary>
        /// Get bounding box of the node
        /// </summary>
        /// <param name="pScene">Scene which the node belongs to</param>
        /// <returns>Axis-aligned bounding box</returns>
        MANDRILL_API AABB getBoundingBox(const ptr<const Scene> pScene) const;

        /// <summary>
        /// Add a mesh to the node.
        /// </summary>
        /// <param name="meshIndex">Mesh index that was received during mesh creation</param>
        MANDRILL_API void addMesh(uint32_t meshIndex)
        {
            mMeshIndices.push_back(meshIndex);
        }

        /// <summary>
        /// Set pipeline to use when rendering node
        /// </summary>
        /// <param name="pPipeline">Pipeline to use</param>
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
        MANDRILL_API void setTransform(glm::mat4 transform)
        {
            mTransform = transform;
        }

        /// <summary>
        /// Set weather the node should be rendered or not.
        /// </summary>
        /// <param name="visible">True to render the node, otherwise false</param>
        MANDRILL_API void setVisible(bool visible)
        {
            mVisible = visible;
        }

        /// <summary>
        /// Get the visibility of the node.
        /// </summary>
        /// <returns>True if visible, otherwise false</returns>
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
        // Index of this node's first element in the scene transform buffer. The node's copy for a frame in flight is
        // that element plus the frame index.
        uint32_t mTransformIndex;

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
        MANDRILL_API Scene(ptr<Device> pDevice);

        /// <summary>
        /// Destructor for scene.
        /// </summary>
        MANDRILL_API ~Scene();

        /// <summary>
        /// Render all the nodes in the scene.
        /// </summary>
        /// <param name="cmd">Command buffer to use for rendering</param>
        /// <param name="pCamera">Camera that defines which camera matrices to use</param>
        /// <param name="frustumCulling">Cull nodes that are outside of the camera's frustum</param>
        /// <param name="frameInFlightIndex">Which copy of the per-frame resources to use, the current frame by
        /// default</param>
        MANDRILL_API void render(VkCommandBuffer cmd, const ptr<Camera> pCamera, bool frustumCulling = true,
                                 uint32_t frameInFlightIndex = kCurrentFrameInFlight) const;

        /// <summary>
        /// Add a node to the scene.
        /// </summary>
        /// <returns>Index to added node</returns>
        MANDRILL_API uint32_t addNode();

        /// <summary>
        /// Add several nodes to the scene by reading them from an OBJ- or GLTF/GLB-file.
        /// </summary>
        /// <param name="path">Path to the file</param>
        /// <param name="materialPath">Path to where the material files are stored (leave to default if the materials
        /// are in the same directory as the OBJ-file, no effect for GLTF/GLB)</param>
        /// <returns>List of indices to nodes that were added</returns>
        MANDRILL_API std::vector<uint32_t> addNodesFromFile(const std::filesystem::path& path,
                                                            const std::filesystem::path& materialPath = "");

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
        /// Calculate sizes of buffers and allocate resources. Call this after all nodes have been added. Node
        /// transforms get one copy per frame in flight, as the device is set up for.
        /// </summary>
        MANDRILL_API void compile();

        /// <summary>
        /// Attach the scene's resources to the shaders its nodes are rendered with.
        ///
        /// Which shaders those are is taken from the pipelines the nodes carry, so every node has to have its
        /// pipeline set before this is called. Nodes may well use different pipelines, and those pipelines may use
        /// different shaders; each distinct shader gets the resources attached to it and a set of materials prepared
        /// for it. Swapping a node to another pipeline afterwards is fine as long as it uses a shader that was
        /// present here, which is the case when the pipelines only differ in state.
        ///
        /// Resources are matched to a shader by the name they are declared with, not by set and binding number, so
        /// the shader decides where they end up. Declare them like this, in any set and at any binding:
        /// <table>
        /// <caption> Resources the scene expects to find in the shader </caption>
        /// <tr><th> Name in shader <th> Contents <th> Declared as <th>
        /// <tr><td> camera <td> Camera matrices (struct CameraMatrices) <td> uniform block named *Dynamic
        /// <tr><td> mesh <td> Node model matrix (mat4) <td> uniform block named *Dynamic
        /// <tr><td> materialParams <td> Material parameters (struct MaterialParams) <td> uniform block
        /// <tr><td> diffuseTexture <td> Material diffuse texture <td> sampler2D
        /// <tr><td> specularTexture <td> Material specular texture <td> sampler2D
        /// <tr><td> ambientTexture <td> Material ambient texture <td> sampler2D
        /// <tr><td> emissionTexture <td> Material emission texture <td> sampler2D
        /// <tr><td> normalTexture <td> Material normal texture <td> sampler2D
        /// <tr><td> environmentMap <td> Environment map texture <td> sampler2D, optional
        /// </table>
        ///
        /// The camera and the node transforms live in one buffer each that is rebound with a dynamic offset, which is
        /// why their blocks have to be named with a *Dynamic suffix. They are bound with different offsets, so they
        /// have to be in separate sets.
        ///
        /// A whole material is bound in one go for every mesh, so the material resources all have to share one set,
        /// declared in the order they are listed above. That set is found from diffuseTexture.
        ///
        /// Only the environment map is optional. A shader that does not declare it simply renders without one.
        /// </summary>
        /// <param name="pCamera">Camera whose matrices the scene is rendered with</param>
        MANDRILL_API void createDescriptors(ptr<Camera> pCamera);

        /// <summary>
        /// Attach the scene's resources to a ray-tracing shader.
        ///
        /// As with createDescriptors(), resources are matched by the name they are declared with rather than by set
        /// and binding number:
        /// <table>
        /// <caption> Resources the scene expects to find in the shader </caption>
        /// <tr><th> Name in shader <th> Contents <th> Declared as <th>
        /// <tr><td> camera <td> Camera matrices (struct CameraMatrices) <td> uniform block named *Dynamic
        /// <tr><td> scene <td> Acceleration structure <td> accelerationStructureEXT
        /// <tr><td> vertexBuffer <td> Global vertex buffer <td> readonly buffer block
        /// <tr><td> indexBuffer <td> Global index buffer <td> readonly buffer block
        /// <tr><td> instanceDataBuffer <td> Vertex and index offsets per instance <td> readonly buffer block
        /// <tr><td> materialBuffer <td> Global material buffer <td> readonly buffer block
        /// <tr><td> textures <td> Global texture array <td> sampler2D array
        /// <tr><td> environmentMap <td> Environment map texture <td> sampler2D, optional
        /// </table>
        ///
        /// A buffer block needs an instance name for the scene to find it, since a block declared without one is only
        /// reachable through its block type name. The texture array has to be sized by a specialization constant that
        /// is set to Scene::getTextureCount().
        ///
        /// Nothing varies per dispatch the way a node transform varies per draw, so the scene has no bind function of
        /// its own here. Bind everything, the scene's resources and the application's alike, with a single call to
        /// Shader::bindResources(cmd, bindPoint) before tracing.
        /// </summary>
        /// <param name="pShader">Shader the scene is traced with, used to attach the resources</param>
        /// <param name="pCamera">Camera whose matrices the scene is traced with</param>
        /// <param name="pAccelerationStructure">Acceleration structure to bind</param>
        MANDRILL_API void createRayTracingDescriptors(ptr<Shader> pShader, ptr<Camera> pCamera,
                                                      const ptr<AccelerationStructure> pAccelerationStructure);

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
        MANDRILL_API void syncToDevice();

        /// <summary>
        /// Get a reference to a node in the scene.
        /// </summary>
        /// <param name="nodeIndex">Index to node</param>
        MANDRILL_API Node& getNode(uint32_t nodeIndex)
        {
            return mNodes[nodeIndex];
        }

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
        /// <returns>Material index</returns>
        MANDRILL_API uint32_t getMeshMaterialIndex(uint32_t meshIndex) const
        {
            return mMeshes[meshIndex].materialIndex;
        }

        /// <summary>
        /// Set an environment map for the scene.
        /// </summary>
        /// <param name="pTexture">Texture to use as environment map</param>
        MANDRILL_API void setEnvironmentMap(ptr<Texture> pTexture)
        {
            mpEnvironmentMap = pTexture;
        }

    private:
        friend Node;

        // What the scene prepared for one of the shaders its nodes are rendered with. Materials are bound as whole
        // sets rather than through the shader, so they have to be allocated against each shader's own layout.
        struct ShaderResources {
            ptr<Shader> pShader;
            uint32_t materialSet = 0;
            std::vector<ptr<Descriptor>> materialDescriptors; // One per material, indexed like mMaterials
        };

        // Attach the scene's resources to one shader and prepare its materials
        void createDescriptorsForShader(ptr<Shader> pShader, ptr<Camera> pCamera);

        // What was prepared for a shader, or nullptr if the scene never saw it
        const ShaderResources* findShaderResources(const Shader* pShader) const;

        std::vector<uint32_t> loadFromOBJ(const std::filesystem::path& path,
                                          const std::filesystem::path& materialPath = "");
        std::vector<uint32_t> loadFromGLTF(const std::filesystem::path& path);
        void addTexture(std::string texturePath);
        void addTextureFromMemory(const uint8_t* pData, size_t size, const std::string& textureName);

        ptr<Device> mpDevice;

        std::vector<Mesh> mMeshes;
        std::vector<Node> mNodes;
        std::vector<Material> mMaterials;
        std::unordered_map<std::string, ptr<Texture>> mTextures;
        ptr<Texture> mpEnvironmentMap;

        ptr<Buffer> mpVertexBuffer;
        ptr<Buffer> mpIndexBuffer;
        // One transform per node and frame in flight, laid out with the frame index varying fastest
        ptr<DynamicBuffer> mpTransforms;
        ptr<Buffer> mpMaterialParams;

        ptr<Texture> mpMissingTexture;

        // One entry per distinct shader among the node pipelines, filled in by createDescriptors()
        std::vector<ShaderResources> mShaderResources;

        ptr<Buffer> mpMaterialBuffer; // Almost same as mpMaterialParams but for ray tracing
        ptr<Buffer> mpInstanceDataBuffer;

        uint32_t mVertexCount;
        uint32_t mIndexCount;
    };
}; // namespace Mandrill
