#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");

    auto cube = engine.loadMesh("assets/models/cube.obj");
    auto material = engine.loadMaterial(
        "assets/textures/albedo.jpg",
        "assets/textures/normal.jpg",
        "assets/textures/metallic.jpg",
        "assets/textures/roughness.jpg");

    auto material2 = engine.loadMaterial(
        "assets/textures/albedo2.jpg",
        "assets/textures/normal2.jpg",
        "assets/textures/metallic2.jpg",
        "assets/textures/roughness2.jpg");

    auto& obj1 = engine.getScene().addObject();
    obj1.mesh = cube;
    obj1.material = material;
    obj1.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);

    auto& obj2 = engine.getScene().addObject();
    obj2.mesh = cube;
    obj2.material = material;
    obj2.transform.position = glm::vec3(3.0f, 0.0f, 0.0f);

    auto& obj3 = engine.getScene().addObject();
    obj3.mesh = cube;
    obj3.material = material;
    obj3.transform.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    obj3.transform.scale = glm::vec3(0.5f);

    auto sphere = engine.loadMesh("assets/models/sphere.obj");
    auto& obj4 = engine.getScene().addObject();
    obj4.mesh = sphere;
    obj4.material = material2;
    obj4.transform.position = glm::vec3(5.0f, 0.0f, 0.0f);
    obj4.transform.rotation = glm::vec3(90.0f, 90.f, 0.0f);

    engine.run();

    engine.shutdown();
    return 0;
}