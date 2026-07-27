#pragma once

#include "Common.h"

namespace Mandrill
{
    struct AABB {
        // An empty box: min > max, so the first expand() call sets both corners to the incoming value. Starting at
        // (0, 0, 0) would force every box to contain the world origin.
        glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());  // Minimum corner of the bounding box
        glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest()); // Maximum corner of the bounding box

        bool empty() const
        {
            return min.x > max.x || min.y > max.y || min.z > max.z;
        }

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

        // Transforming only the min and max corners is only valid for axis-aligned scale and translation. Under a
        // rotation the transformed corners are no longer the extremes (min can even end up greater than max), so all
        // eight corners have to be transformed and the extremes recomputed.
        void transform(const glm::mat4& transform)
        {
            if (empty()) {
                return;
            }

            const glm::vec3 corners[8] = {
                {min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {max.x, max.y, min.z},
                {min.x, min.y, max.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z},
            };

            AABB transformed;
            for (const auto& corner : corners) {
                transformed.expand(glm::vec3(transform * glm::vec4(corner, 1.0f)));
            }

            *this = transformed;
        }

        static struct AABB calculate(const std::vector<glm::vec3>& points)
        {
            AABB aabb;
            for (const auto& point : points) {
                aabb.expand(point);
            }
            return aabb;
        }
    };
} // namespace Mandrill
