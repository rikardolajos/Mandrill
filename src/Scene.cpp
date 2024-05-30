#include "Scene.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

using namespace Mandrill;

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::render(VkCommandBuffer cmd) const
{
}

Mesh Scene::addMesh(const std::filesystem::path& path)
{
    return Mesh();
}