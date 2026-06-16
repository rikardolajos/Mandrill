#pragma once

#include "Common.h"

namespace Mandrill
{
    struct AABB {
        glm::vec3 min; // Minimum corner of the bounding box
        glm::vec3 max; // Maximum corner of the bounding box

        void expand(const struct AABB aabb)
        {
            min = glm::min(min, aabb.min);
            max = glm::max(max, aabb.max);
        }

        void expand(const glm::vec3 point)
        {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void transform(const glm::mat4& transform)
        {
            min = transform * glm::vec4(min, 1.0f);
            max = transform * glm::vec4(max, 1.0f);
        }

        static struct AABB calculate(const std::vector<glm::vec3>& points)
        {
            AABB aabb;
            if (points.empty()) {
                aabb.min = glm::vec3(0.0f);
                aabb.max = glm::vec3(0.0f);
                return aabb;
            }

            aabb.min = points[0];
            aabb.max = points[0];

            for (const auto& point : points) {
                aabb.min = glm::min(aabb.min, point);
                aabb.max = glm::max(aabb.max, point);
            }

            return aabb;
        }
    };
} // namespace Mandrill
