#pragma once
#include "vk_types.h"

class PipelineBuilder {
public:
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
   
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencil;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentformat;

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
    void enableBlendingAdditive();
    void enableBlendingAlpha();
     
    // disable features
    void disableBlending();
    void disableDepthtest();
};

namespace vkutil {
    bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
};