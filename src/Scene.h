#pragma once

#include "Common.h"

namespace Mandrill
{
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uvCoord;
    };

    class MANDRILL_API Mesh
    {
    public:
    private:
        uint32_t materialIndex;
    };

    class MANDRILL_API Material
    {
    public:
    private:
    };

    class Scene
    {
    public:
        MANDRILL_API Scene();
        MANDRILL_API ~Scene();

        MANDRILL_API void render(VkCommandBuffer cmd) const;

        MANDRILL_API Mesh addMesh(const std::filesystem::path& path);


    private:
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
    };
}; // namespace Mandrill
