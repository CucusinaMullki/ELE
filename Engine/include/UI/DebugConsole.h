#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>

class DebugConsole
{
public:
    static DebugConsole& instance()
    {
        static DebugConsole inst;
        return inst;
    }

    void log(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.push_back(message);
        if (m_entries.size() > m_maxEntries)
            m_entries.pop_front();
    }

    const std::deque<std::string>& getEntries() const { return m_entries; }

    void clear() { std::lock_guard<std::mutex> lock(m_mutex); m_entries.clear(); }

private:
    std::deque<std::string> m_entries;
    size_t m_maxEntries = 500;
    std::mutex m_mutex;
};
