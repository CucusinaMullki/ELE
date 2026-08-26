#pragma once
#include "UI/Widget.h"
#include "Vulkan/Texture.h"
#include <imgui_impl_vulkan.h>
#include <memory>

class ImageWidget : public Widget
{
public:
    ImageWidget(std::shared_ptr<Texture> texture) : m_texture(texture) 
    {
        m_descriptorSet = ImGui_ImplVulkan_AddTexture(
            (VkSampler)m_texture->getSampler(),
            (VkImageView)m_texture->getImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    ~ImageWidget()
    {
        if (m_descriptorSet)
            ImGui_ImplVulkan_RemoveTexture(m_descriptorSet);
    }

    void draw() override
    {
        ImGui::SetNextWindowPos(ImVec2(m_position.x, m_position.y));
        ImGui::SetNextWindowSize(ImVec2(m_size.x, m_size.y));
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

        ImGui::Begin(("##image_" + std::to_string((uintptr_t)this)).c_str(), nullptr, flags);
        ImGui::Image((ImTextureID)m_descriptorSet, ImVec2(m_size.x, m_size.y));
        ImGui::End();
    }

private:
    std::shared_ptr<Texture> m_texture;
    VkDescriptorSet m_descriptorSet = nullptr;
};