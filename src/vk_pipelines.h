#pragma once
#include "vk_types.h"

class PipelineBuilder {
public:
    VkFormat _colorAttachmentformat;

    VkPipelineLayout _pipelineLayout;

    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    /**
     * 
     */
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;

	PipelineBuilder(){ clear(); }

    void clear();

    VkPipeline buildPipeline(VkDevice device);

    // set pipeline
    void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void setInputTopology(VkPrimitiveTopology topology);
    void setPolygonMode(VkPolygonMode mode);
    void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    void setMultisamplingNone();
    void setColorAttachmentFormat(VkFormat format);
    void setDepthFormat(VkFormat format);

    // enable features
    void enableMultisampling(VkSampleCountFlagBits sampleCount);
    void enableDepthTest(bool depthWriteEnable, VkCompareOp op);
    void enableStencilTest(VkStencilOpState front, VkStencilOpState back);
    void enableBlendingAdditive();
    void enableBlendingAlpha();
     
    // disable features
    void disableBlending();
    void disableDepthtest();
};

namespace vkutil {
    bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};