#include "Nav/GeometryParser.h"
#include <cmath>

WalkabilityResult GeometryParser::isWalkable(PhysicsSystem& physics, glm::vec3 point,
    float maxSlopeDegrees, float characterHeight, float rayStartHeight, float maxRayDistance)
{
    WalkabilityResult result{};

    glm::vec3 rayOrigin(point.x, point.y + rayStartHeight, point.z);
    RaycastHit hit = physics.raycast(rayOrigin, glm::vec3(0.0f, -1.0f, 0.0f), maxRayDistance);

    if (!hit.hit)
        return result;

    float slopeAngle = glm::degrees(acos(glm::clamp(glm::dot(hit.normal, glm::vec3(0, 1, 0)), -1.0f, 1.0f)));

    if (slopeAngle > maxSlopeDegrees)
        return result;

    glm::vec3 clearanceCheckOrigin = hit.point + glm::vec3(0.0f, 0.05f, 0.0f);
    RaycastHit ceilingHit = physics.raycast(clearanceCheckOrigin, glm::vec3(0.0f, 1.0f, 0.0f), characterHeight);

    if (ceilingHit.hit)
        return result;

    result.walkable = true;
    result.groundPoint = hit.point;
    return result;
}