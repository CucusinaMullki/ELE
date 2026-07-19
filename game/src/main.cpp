#include "Core/Engine.h"

int main()
{
    Engine engine;
    engine.initialize(800, 600, "ELE Game");

    //Add meshes//////////////////
    auto cube = engine.loadMesh("assets/models/cube.glb");
    auto sphere = engine.loadMesh("assets/models/sphere.glb");
    //////////////////////////////

    //Add materials///////////////
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
    ///////////////////////////////

    //Add lights(directional and point light)
    engine.addPointLight(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.03f, 1.91f, 2.55f), 15.0f);

    //Making objects with mesh and material
    auto& obj1 = engine.getScene().addObject();
    obj1.mesh = cube;
    obj1.setMaterial(material);
    obj1.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);

    auto& obj2 = engine.getScene().addObject();
    obj2.mesh = cube;
    obj2.setMaterial(material);
    obj2.transform.position = glm::vec3(3.0f, 0.0f, 0.0f);

    auto& obj3 = engine.getScene().addObject();
    obj3.mesh = cube;
    obj3.setMaterial(material);
    obj3.transform.position = glm::vec3(-3.0f, 0.0f, 0.0f);
    obj3.transform.scale = glm::vec3(0.5f);

    auto& obj4 = engine.getScene().addObject();
    obj4.mesh = sphere;
    obj4.setMaterial(material);
    obj4.transform.position = glm::vec3(5.0f, 0.0f, 0.0f);

    auto& character = engine.loadGltfObject("assets/models/BlenderModel.glb");
    character.transform.position = glm::vec3(0.0f, 2.0f, 0.0f);
    //////////////////////////////
        
    engine.run();

    engine.shutdown();
    return 0;
}