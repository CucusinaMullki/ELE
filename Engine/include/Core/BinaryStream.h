#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
#include <type_traits>

class BinaryWriter
{
public:
    template<typename T>
    void write(const T& v)
    {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter::write: POD types only");
        const size_t at = m_data.size();
        m_data.resize(at + sizeof(T));
        std::memcpy(m_data.data() + at, &v, sizeof(T));
    }

    void writeString(const std::string& s)
    {
        write<uint32_t>((uint32_t)s.size());
        writeBytes(s.data(), s.size());
    }

    void writeBytes(const void* src, size_t n)
    {
        const size_t at = m_data.size();
        m_data.resize(at + n);
        if (n) std::memcpy(m_data.data() + at, src, n);
    }

    size_t beginBlock()
    {
        const size_t at = m_data.size();
        write<uint32_t>(0);
        return at;
    }

    void endBlock(size_t at)
    {
        patch<uint32_t>(at, (uint32_t)(m_data.size() - at - sizeof(uint32_t)));
    }

    template<typename T>
    void patch(size_t at, const T& v)
    {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryWriter::patch: POD types only");
        std::memcpy(m_data.data() + at, &v, sizeof(T));
    }

    size_t size() const { return m_data.size(); }
    const std::vector<uint8_t>& data() const { return m_data; }

    bool saveToFile(const std::string& path) const
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(reinterpret_cast<const char*>(m_data.data()), (std::streamsize)m_data.size());
        return (bool)f;
    }

private:
    std::vector<uint8_t> m_data;
};

class BinaryReader
{
public:
    BinaryReader() = default;
    BinaryReader(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    template<typename T>
    T read()
    {
        static_assert(std::is_trivially_copyable_v<T>, "BinaryReader::read: POD types only");
        T v{};
        if (!require(sizeof(T))) return v;
        std::memcpy(&v, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return v;
    }

    std::string readString()
    {
        const uint32_t n = read<uint32_t>();
        if (!require(n)) return {};
        std::string s(reinterpret_cast<const char*>(m_data + m_pos), n);
        m_pos += n;
        return s;
    }

    BinaryReader readBlock()
    {
        const uint32_t n = read<uint32_t>();
        if (!require(n)) return {};
        BinaryReader sub(m_data + m_pos, n);
        m_pos += n;
        return sub;
    }

    bool skip(size_t n) { if (!require(n)) return false; m_pos += n; return true; }

    const uint8_t* cursor() const { return m_data + m_pos; }
    size_t tell() const { return m_pos; }
    size_t remaining() const { return m_size - m_pos; }
    bool eof() const { return m_pos >= m_size; }
    bool ok() const { return m_ok; }

    static bool loadFile(const std::string& path, std::vector<uint8_t>& out)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return false;
        const auto n = (size_t)f.tellg();
        f.seekg(0);
        out.resize(n);
        f.read(reinterpret_cast<char*>(out.data()), (std::streamsize)n);
        return (bool)f;
    }

private:
    bool require(size_t n)
    {
        if (!m_ok || m_pos + n > m_size) { m_ok = false; return false; }
        return true;
    }

    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_pos = 0;
    bool m_ok = true;
};
