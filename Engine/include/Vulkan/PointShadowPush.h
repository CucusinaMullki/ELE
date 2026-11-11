#include <glm/glm.hpp>

struct PointShadowPush
{
    glm::mat4 lightViewProj;
    glm::mat4 model;
    glm::vec4 lightPos;
};
