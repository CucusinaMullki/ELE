#include "Core/AnimationLoader.h"
#include "Core/Path.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace
{
    template<typename T>
    T readValue(const uint8_t* data, size_t& offset)
    {
        T value{};
        std::memcpy(&value, data + offset, sizeof(T));
        offset += sizeof(T);
        return value;
    }

    std::string reeadString(const uint8_t* data, size_t& offset)
    {
        uint8_t length = readValue<uint32_t>(data, offset);
        std::string result(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        return result;
    }
}

Animation AnimationLoader::load(const std::string& path)
{
    std::string fullPath = (Paths::assetDir() / path).string();

    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("failed to open animation file: " + fullPath);

    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);

    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    const uint8_t* data = buffer.data();
    size_t offset = 0;

    char magic[5] = {0};
    std::memcpy(magic, data, 4);
    offset += 4;
    if (std::strcmp(magic, "EANM") != 0)
        throw std::runtime_error("invalid animation file magic: " + fullPath);

    uint32_t version = readValue<uint32_t>(data, offset);
    (void)version;

    Animation anim;
    anim.name = reeadString(data, offset);
    anim.duration = readValue<float>(data, offset);

    uint32_t channelCount = readValue<uint32_t>(data, offset);
    anim.channels.resize(channelCount);

    for (auto& channel : anim.channels)
    {
        channel.jointName = reeadString(data, offset);

        uint32_t posCount = readValue<uint32_t>(data, offset);
        channel.poisitonKeys.resize(posCount);
        for (auto& key : channel.poisitonKeys)
        {
            key.time = readValue<float>(data, offset);
            key.value = readValue<glm::vec3>(data, offset);
        }

        uint32_t rotCount = readValue<uint32_t>(data, offset);
        channel.rotationKeys.resize(rotCount);
        for (auto& key : channel.rotationKeys)
        {
            key.time = readValue<float>(data, offset);
            float x = readValue<float>(data, offset);
            float y = readValue<float>(data, offset);
            float z = readValue<float>(data, offset);
            float w = readValue<float>(data, offset);
            key.value = glm::quat(w, x, y, z);
        }

        uint32_t scaleCount = readValue<uint32_t>(data, offset);
        channel.scaleKeys.resize(scaleCount);
        for (auto& key : channel.scaleKeys)
        {
            key.time = readValue<float>(data, offset);
            key.value = readValue<glm::vec3>(data, offset);
        }
    }

    return anim;
}