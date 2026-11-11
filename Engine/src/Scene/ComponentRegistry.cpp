#include "Scene/ComponentRegistry.h"
#include "Core/Log.h"
#include <algorithm>

ComponentRegistry& ComponentRegistry::instance()
{
    static ComponentRegistry r;
    return r;
}

void ComponentRegistry::add(const char* name, Factory f)
{
    const uint32_t h = propHash(name);
    auto it = m_entries.find(h);
    if (it != m_entries.end())
    {
        LOG("ComponentRegistry: '" << name << "' clashes with '" << it->second.name << "'");
        return;
    }
    m_entries.emplace(h, Entry{ name, std::move(f) });
    m_order.push_back(h);
}

const ComponentRegistry::Entry* ComponentRegistry::find(uint32_t hash) const
{
    auto it = m_entries.find(hash);
    return it == m_entries.end() ? nullptr : &it->second;
}

void ComponentRegistry::remove(const std::string& name)
{
    const uint32_t h = propHash(name.c_str());
    m_entries.erase(h);
    m_order.erase(std::remove(m_order.begin(), m_order.end(), h), m_order.end());
}
