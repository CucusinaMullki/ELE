#pragma once
#include "Scene/ComponentRegistry.h"
#include "Scene/GameObject.h"
#include <cstdint>
#include <vector>

#if defined(_WIN32)
#  define ELE_SCRIPT_EXPORT extern "C" __declspec(dllexport)
#else
#  define ELE_SCRIPT_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace ele
{
    constexpr uint32_t kScriptApiVersion = 1;

    struct ScriptTypeEntry
    {
        const char* name;
        ComponentRegistry::Factory factory;
    };

    inline std::vector<ScriptTypeEntry>& moduleScriptTypes()
    {
        static std::vector<ScriptTypeEntry> types;
        return types;
    }

    template<typename T>
    struct ScriptRegistrar
    {
        explicit ScriptRegistrar(const char* name)
        {
            moduleScriptTypes().push_back({ name, [](GameObject& o, EngineContext&) -> IComponent* {
                return &o.addComponent<T>();
            } });
        }
    };
}

#define ELE_SCRIPT(Type) \
    const char* typeName() const override { return #Type; }

#define ELE_REGISTER_SCRIPT(Type) \
    static ::ele::ScriptRegistrar<Type> ele_script_registrar_##Type(#Type);

#define ELE_SCRIPT_MODULE()                                                                 \
    ELE_SCRIPT_EXPORT uint32_t ele_script_api_version() { return ::ele::kScriptApiVersion; } \
    ELE_SCRIPT_EXPORT uint32_t ele_register_scripts(ComponentRegistry* r,                     \
                                                    const char** names, uint32_t cap)         \
    {                                                                                         \
        uint32_t n = 0;                                                                       \
        for (const auto& t : ::ele::moduleScriptTypes())                                      \
        {                                                                                     \
            r->add(t.name, t.factory);                                                        \
            if (n < cap) names[n] = t.name;                                                   \
            ++n;                                                                              \
        }                                                                                     \
        return n;                                                                             \
    }
