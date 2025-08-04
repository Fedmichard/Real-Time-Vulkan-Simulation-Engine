#include "vk_descriptors.h"
#include "vk_types.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStageFlag) {
    // every binding needs to be described
    // it can take in a list of bindings
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = binding;
    layoutBinding.descriptorCount = 1;
    layoutBinding.descriptorType = type;
    layoutBinding.stageFlags = shaderStageFlag;

    bindings.push_back(layoutBinding);
}

void DescriptorLayoutBuilder::clear() {
    bindings.clear();
}


VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, void* pNext, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();
    createInfo.flags = flags;

    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout));

    return descriptorSetLayout;
}

// get pool from our ready pools
VkDescriptorPool DescriptorAllocator2::getPool(VkDevice device) {       
    VkDescriptorPool newPool;

    // if we have a ready pool, grab the one from the back and remove it from the ready pools vector
    if (readyPools.size() != 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    // else create a new pool
    } else {
	    //need to create a new pool
	    newPool = createPool(device, setsPerPool, ratios);

        // max sets per pool (this is essentially a vector.resize() for our class)
	    setsPerPool = setsPerPool * 1.5;
	    if (setsPerPool > 4092) {
		    setsPerPool = 4092;
	    }
    }   

    return newPool;
}

VkDescriptorPool DescriptorAllocator2::createPool(VkDevice device, uint32_t maxSets, std::vector<PoolSizeRatio> poolRatios) {
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * maxSets)
		});
	}

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.maxSets = maxSets;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
    return newPool;
}

/**
 * creates a pool that is preallocated for a certain amount of max sets
 *          meaning this memory region can create 10 sets of similar 
 *          binding points and descriptors
 */
void DescriptorAllocator2::init(VkDevice device, uint32_t maxSets, std::vector<PoolSizeRatio> poolRatios) {
    ratios.clear();
    
    for (auto r : poolRatios) {
        ratios.push_back(r);
    }
	
    VkDescriptorPool newPool = createPool(device, maxSets, poolRatios);

    setsPerPool = maxSets * 1.5; //grow it next allocation

    readyPools.push_back(newPool);
}

/**
 * clears all the available allocated pools and full allocated pools then adds full pools back to ready pool
 */
void DescriptorAllocator2::clearPools(VkDevice device) { 
    for (auto p : readyPools) {
        vkResetDescriptorPool(device, p, 0);
    }
    for (auto p : fullPools) {
        vkResetDescriptorPool(device, p, 0);
        readyPools.push_back(p);
    }
    fullPools.clear();
}

void DescriptorAllocator2::destroyPools(VkDevice device) {
	for (auto p : readyPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
    readyPools.clear();
	for (auto p : fullPools) {
		vkDestroyDescriptorPool(device,p,nullptr);
    }
    fullPools.clear();
}

/**
* once a pool is actually initialized and created, you can allocate descriptor sets from that pool,
* if your pool can create 10 max sets you can allocate 10 times (for your sets) from that memory region 
*           Getpool just takes the most available pool from the back of readyPools
*           at that point your individual descriptor set is created but it is never written to so it's more or less empty
*/
VkDescriptorSet DescriptorAllocator2::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext) {
    //get or create a pool to allocate from
    VkDescriptorPool poolToUse = getPool(device);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = poolToUse;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet ds;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

    //allocation failed. Try again
    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

        fullPools.push_back(poolToUse);
    
        poolToUse = getPool(device);
        allocInfo.descriptorPool = poolToUse;

       VK_CHECK( vkAllocateDescriptorSets(device, &allocInfo, &ds));
    }
  
    readyPools.push_back(poolToUse);
    return ds;
}

void DescriptorWriter::writeImage(int binding, VkImageView image, VkSampler sampler ,VkImageLayout layout, VkDescriptorType type) {
    // first create resource info
    // place it into image info vector
    VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout
    });

    // then create descriptor write
	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

    // add it to our writes
	descriptorWrites.push_back(write);
}

void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    // first create resource info
    // place it into buffer info vector
    VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
    });

    // then create descriptor write
	VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE; // left empty for now until we need to write it
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

    // add it to our writes
	descriptorWrites.push_back(write);
}

void DescriptorWriter::clear() {
    imageInfos.clear();
    descriptorWrites.clear();
    bufferInfos.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet& write : descriptorWrites) {
        write.dstSet = set;
    }

    // allocates 
    vkUpdateDescriptorSets(device, (uint32_t)descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}