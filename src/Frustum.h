#pragma once

#include "Common.h"

#include "AABB.h"

namespace Mandrill
{
    struct Frustum {
        glm::vec4 planes[6]; // Left, Right, Top, Bottom, Near, Far

        bool intersects(const AABB& aabb) const
        {
            for (const auto& plane : planes) {
                glm::vec3 normal = glm::vec3(plane);
                float distance = plane.w;
                glm::vec3 positiveVertex = aabb.min;
                if (normal.x >= 0.0f)
                    positiveVertex.x = aabb.max.x;
                if (normal.y >= 0.0f)
                    positiveVertex.y = aabb.max.y;
                if (normal.z >= 0.0f)
                    positiveVertex.z = aabb.max.z;
                if (glm::dot(normal, positiveVertex) + distance < 0.0f) {
                    return false; // AABB is outside this plane
                }
            }
            return true; // AABB is inside or intersects the frustum
        }
    };
} // namespace Mandrill
