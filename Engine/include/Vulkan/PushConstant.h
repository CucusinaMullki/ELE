#pragma once
#include <glm/glm.hpp>

struct PushConstant
{
    glm::mat4 model;
    glm::mat4 normalMatrix;
    glm::mat4 prevModel;
};
