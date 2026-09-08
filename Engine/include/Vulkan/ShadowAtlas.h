#pragma once
#include "ASH/ASH.h"
#include <memory>

class ShadowAtlas
{
public:
    static constexpr uint32_t TILE_SIZE = 512;
    static constexpr uint32_t TILES_X = 6;
    static constexpr uint32_t TILES_Y = 3;

    void initialize(ASH::Device* device);
    void cleanup();

    ASH::Texture* getTexture() const { return m_texture.get(); }
    ASH::Sampler* getSampler() const { return m_sampler.get(); }
    ASH::Format getFormat() const { return m_format; }
    uint32_t getWidth() const { return TILE_SIZE * TILES_X; }
    uint32_t getHeight() const { return TILE_SIZE * TILES_Y; }

    ASH::Rect2D getTileRect(uint32_t lightSlot, uint32_t face) const
    {
        ASH::Rect2D rect{};
        rect.x = (int32_t)(face * TILE_SIZE);
        rect.y = (int32_t)(lightSlot * TILE_SIZE);
        rect.width = TILE_SIZE;
        rect.height = TILE_SIZE;
        return rect;
    }

private:
    std::unique_ptr<ASH::Texture> m_texture;
    std::unique_ptr<ASH::Sampler> m_sampler;
    ASH::Format m_format = ASH::Format::D32_Float;
};