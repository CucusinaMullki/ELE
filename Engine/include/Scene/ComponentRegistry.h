#pragma once
#include "Scene/Property.h"
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class IComponent;
class GameObject;
class PhysicsSystem;
class Input;
class TimeSystem;
class Camera;
class AudioSystem;
namespace ASH { class Device; }

struct EngineContext
{
    ASH::Device* device = nullptr;
    PhysicsSystem* physics = nullptr;
    AudioSystem* audio = nullptr;
    Input* input = nullptr;
    TimeSystem* time = nullptr;
    Camera* camera = nullptr;
    uint32_t framesInFlight = 0;
};

class ComponentRegistry
{
public:
    using Factory = std::function<IComponent*(GameObject&, EngineContext&)>;

    struct Entry
    {
        std::string name;
        Factory     create;
    };

    static ComponentRegistry& instance();

    void add(const char* name, Factory f);
    void remove(const std::string& name);

    const Entry* find(uint32_t hash) const;
    const Entry* find(const char* name) const { return find(propHash(name)); }

    const std::vector<uint32_t>& order() const { return m_order; }

private:
    std::unordered_map<uint32_t, Entry> m_entries;
    std::vector<uint32_t> m_order;
};

void registerEngineComponents(ComponentRegistry& r);
