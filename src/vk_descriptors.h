#pragma once

// #include "vk_types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <deque>

// build a descriptor set layout
struct DescriptorLayoutBuilder {
    // bindings
    std::vector<VkDescriptorSetLayoutBinding> bindings; // a vector of all the descriptor set layout bindings we'll use

    // functions
    void addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStageFlag); // add a binding to our vector
    void clear();
    VkDescriptorSetLayout build(VkDevice device, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

// descriptor allocator 2.0
struct DescriptorAllocator2 {
public:
	struct PoolSizeRatio {
		VkDescriptorType type;
		float ratio;
	};

	void init(VkDevice device, uint32_t initialSets, std::vector<PoolSizeRatio> poolRatios);
	void clearPools(VkDevice device);
	void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
	VkDescriptorPool getPool(VkDevice device);
	VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::vector<PoolSizeRatio> poolRatios);

	std::vector<PoolSizeRatio> ratios;
	std::vector<VkDescriptorPool> fullPools;
	std::vector<VkDescriptorPool> readyPools;
	uint32_t setsPerPool;

};

struct DescriptorWriter {
    // each descriptor within a set that we may have
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;

    // once we've allocated a descriptor set we need to update it's contents now
    // updates each binding
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    void writeImage(int binding,VkImageView image,VkSampler sampler , VkImageLayout layout, VkDescriptorType type);
    void writeBuffer(int binding,VkBuffer buffer,size_t size, size_t offset,VkDescriptorType type); 

    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);
};