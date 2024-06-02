#include "Scene.h"

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