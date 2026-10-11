#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float primaryT;

void main()
{
    primaryT = -1.0;
}