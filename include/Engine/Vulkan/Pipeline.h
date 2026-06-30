#include "vulkan/vulkan.hpp"
#include <fstream>

class Pipeline
{
public:
    void initialize(vk::Device device, vk::Extent2D swapchainExtent, vk::RenderPass renderPass);
    void cleanup();

    vk::Pipeline getPipeline() const { return m_graphicPipeline.get(); } 
    vk::PipelineLayout getLayout() const { return m_pipelineLayout.get(); } 
private:

    std::vector<char> readFile(const std::string& filename);
    vk::UniqueShaderModule createModule(vk::Device device, const std::vector<char>& code);

    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_graphicPipeline;
};