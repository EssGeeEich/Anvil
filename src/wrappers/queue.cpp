//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "misc/debug.h"
#include "misc/object_tracker.h"
#include "misc/window.h"
#include "wrappers/buffer.h"
#include "wrappers/command_buffer.h"
#include "wrappers/device.h"
#include "wrappers/fence.h"
#include "wrappers/instance.h"
#include "wrappers/queue.h"
#include "wrappers/rendering_surface.h"
#include "wrappers/semaphore.h"
#include "wrappers/swapchain.h"

#define MAX_SWAPCHAINS (32)


/** Please see header for specification */
Anvil::Queue::Queue(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                    uint32_t                         queue_family_index,
                    uint32_t                         queue_index)

    :m_device_ptr        (device_ptr),
     m_queue             (VK_NULL_HANDLE),
     m_queue_family_index(queue_family_index),
     m_queue_index       (queue_index)
{
    std::shared_ptr<Anvil::BaseDevice> device_locked_ptr(device_ptr);

    /* Retrieve the Vulkan handle */
    vkGetDeviceQueue(device_locked_ptr->get_device_vk(),
                     queue_family_index,
                     queue_index,
                    &m_queue);

    anvil_assert(m_queue != VK_NULL_HANDLE);

    /* Determine whether the queue supports sparse bindings */
    m_supports_sparse_bindings = !!(device_locked_ptr->get_queue_family_info(queue_family_index)->flags & VK_QUEUE_SPARSE_BINDING_BIT);

    /* OK, register the wrapper instance and leave */
    Anvil::ObjectTracker::get()->register_object(Anvil::OBJECT_TYPE_QUEUE,
                                                  this);
}

/** Please see header for specification */
Anvil::Queue::~Queue()
{
    /* Queues are indestructible. Nothing to do here. */

    Anvil::ObjectTracker::get()->unregister_object(Anvil::OBJECT_TYPE_QUEUE,
                                                    this);
}

/** Please see header for specification */
bool Anvil::Queue::bind_sparse_memory(Anvil::Utils::SparseMemoryBindingUpdateInfo& update)
{
    const VkBindSparseInfo*       bind_info_items   = nullptr;
    std::shared_ptr<Anvil::Fence> fence_ptr;
    uint32_t                      n_bind_info_items = 0;
    VkResult                      result            = VK_ERROR_INITIALIZATION_FAILED;

    update.get_bind_sparse_call_args(&n_bind_info_items,
                                     &bind_info_items,
                                     &fence_ptr);

    result = vkQueueBindSparse(m_queue,
                               n_bind_info_items,
                               bind_info_items,
                               (fence_ptr != nullptr) ? fence_ptr->get_fence() : VK_NULL_HANDLE);
    anvil_assert(result == VK_SUCCESS);

    for (uint32_t n_bind_info = 0;
                  n_bind_info < n_bind_info_items;
                ++n_bind_info)
    {
        uint32_t n_buffer_memory_updates       = 0;
        uint32_t n_image_memory_updates        = 0;
        uint32_t n_image_opaque_memory_updates = 0;

        update.get_bind_info_properties(n_bind_info,
                                       &n_buffer_memory_updates,
                                       &n_image_memory_updates,
                                       &n_image_opaque_memory_updates,
                                        nullptr,  /* opt_out_n_signal_semaphores_ptr   */
                                        nullptr,  /* opt_out_signal_semaphores_ptr_ptr */
                                        nullptr,  /* opt_out_n_wait_semaphores_ptr     */
                                        nullptr); /* opt_out_wait_semaphores_ptr_ptr   */

        for (uint32_t n_buffer_memory_update = 0;
                      n_buffer_memory_update < n_buffer_memory_updates;
                    ++n_buffer_memory_update)
        {
            VkDeviceSize                        alloc_size                 = UINT64_MAX;
            VkDeviceSize                        buffer_memory_start_offset = UINT64_MAX;
            std::shared_ptr<Anvil::Buffer>      buffer_ptr;
            std::shared_ptr<Anvil::MemoryBlock> memory_block_ptr;
            VkDeviceSize                        memory_block_start_offset;

            update.get_buffer_memory_update_properties(n_bind_info,
                                                       n_buffer_memory_update,
                                                      &buffer_ptr,
                                                      &buffer_memory_start_offset,
                                                      &memory_block_ptr,
                                                      &memory_block_start_offset,
                                                       &alloc_size);

            buffer_ptr->set_memory_sparse(memory_block_ptr,
                                          memory_block_start_offset,
                                          buffer_memory_start_offset,
                                          alloc_size);
        }

        for (uint32_t n_image_memory_update = 0;
                      n_image_memory_update < n_image_memory_updates;
                    ++n_image_memory_update)
        {
            std::shared_ptr<Anvil::Image>       image_ptr;
            VkExtent3D                          extent;
            VkSparseMemoryBindFlags             flags;
            std::shared_ptr<Anvil::MemoryBlock> memory_block_ptr;
            VkDeviceSize                        memory_block_start_offset;
            VkOffset3D                          offset;
            VkImageSubresource                  subresource;

            update.get_image_memory_update_properties(n_bind_info,
                                                      n_image_memory_update,
                                                      &image_ptr,
                                                      &subresource,
                                                      &offset,
                                                      &extent,
                                                      &flags,
                                                      &memory_block_ptr,
                                                      &memory_block_start_offset);


            image_ptr->set_memory_sparse(subresource,
                                         offset,
                                         extent,
                                         memory_block_ptr,
                                         memory_block_start_offset);
        }

        for (uint32_t n_image_opaque_memory_update = 0;
                      n_image_opaque_memory_update < n_image_opaque_memory_updates;
                    ++n_image_opaque_memory_update)
        {
            VkSparseMemoryBindFlags             flags;
            std::shared_ptr<Anvil::Image>       image_ptr;
            std::shared_ptr<Anvil::MemoryBlock> memory_block_ptr;
            VkDeviceSize                        memory_block_start_offset;
            VkDeviceSize                        resource_offset;
            VkDeviceSize                        size;

            update.get_image_opaque_memory_update_properties(n_bind_info,
                                                             n_image_opaque_memory_update,
                                                            &image_ptr,
                                                            &resource_offset,
                                                            &size,
                                                            &flags,
                                                            &memory_block_ptr,
                                                            &memory_block_start_offset);

            image_ptr->set_memory_sparse(resource_offset,
                                         size,
                                         memory_block_ptr,
                                         memory_block_start_offset);
        }
    }

    return (result == VK_SUCCESS);
}

/** Please see header for specification */
std::shared_ptr<Anvil::Queue> Anvil::Queue::create(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                                                   uint32_t                         queue_family_index,
                                                   uint32_t                         queue_index)
{
    std::shared_ptr<Queue> result_ptr;

    result_ptr.reset(
        new Anvil::Queue(device_ptr,
                         queue_family_index,
                         queue_index)
    );

    return result_ptr;
}

/** Please see header for specification */
VkResult Anvil::Queue::present(std::shared_ptr<Anvil::Swapchain>  swapchain_ptr,
                               uint32_t                           swapchain_image_index,
                               uint32_t                           n_wait_semaphores,
                               std::shared_ptr<Anvil::Semaphore>* wait_semaphore_ptrs)
{
    std::shared_ptr<Anvil::BaseDevice> device_locked_ptr      (m_device_ptr);
    VkPresentInfoKHR                   image_presentation_info;
    VkResult                           presentation_results   [MAX_SWAPCHAINS];
    VkResult                           result;
    const auto&                        swapchain_entrypoints (m_device_ptr.lock()->get_extension_khr_swapchain_entrypoints() );
    VkSwapchainKHR                     swapchains_vk          [MAX_SWAPCHAINS];
    VkSemaphore                        wait_semaphores_vk     [8];

    /* Sanity checks */
    anvil_assert(n_wait_semaphores < sizeof(wait_semaphores_vk) / sizeof(wait_semaphores_vk[0]) );

    /* If the application is only interested in off-screen rendering, do *not* post the present request,
     * since the fake swapchain image is not presentable. We still have to wait on the user-specified
     * semaphores though. */
    if (swapchain_ptr != nullptr)
    {
        std::shared_ptr<Anvil::Window> window_locked_ptr;
        std::weak_ptr<Anvil::Window>   window_ptr;

        window_ptr = swapchain_ptr->get_window();

        if (!window_ptr.expired() )
        {
            window_locked_ptr = window_ptr.lock();
        }

        if (window_locked_ptr != nullptr)
        {
            const WindowPlatform window_platform = window_locked_ptr->get_platform();

            if (window_platform == WINDOW_PLATFORM_DUMMY                    ||
                window_platform == WINDOW_PLATFORM_DUMMY_WITH_PNG_SNAPSHOTS)
            {
                static const VkPipelineStageFlags dst_stage_mask(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

                device_locked_ptr->get_universal_queue(0)->submit_command_buffer_with_wait_semaphores(nullptr, /* cmd_buffer_ptr */
                                                                                                      n_wait_semaphores,
                                                                                                      wait_semaphore_ptrs,
                                                                                                     &dst_stage_mask,
                                                                                                      true); /* should_block */

                result = VK_SUCCESS;
                goto end;
            }
        }
    }

    swapchains_vk[0] = swapchain_ptr->get_swapchain_vk();

    for (uint32_t n_wait_semaphore = 0;
                  n_wait_semaphore < n_wait_semaphores;
                ++n_wait_semaphore)
    {
        wait_semaphores_vk[n_wait_semaphore] = wait_semaphore_ptrs[n_wait_semaphore]->get_semaphore();
    }

    image_presentation_info.pImageIndices      = &swapchain_image_index;
    image_presentation_info.pNext              = nullptr;
    image_presentation_info.pResults           = presentation_results;
    image_presentation_info.pSwapchains        = swapchains_vk;
    image_presentation_info.pWaitSemaphores    = wait_semaphores_vk;
    image_presentation_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    image_presentation_info.swapchainCount     = 1;
    image_presentation_info.waitSemaphoreCount = n_wait_semaphores;

    result = swapchain_entrypoints.vkQueuePresentKHR(m_queue,
                                                    &image_presentation_info);

    anvil_assert_vk_call_succeeded(result);

    if (is_vk_call_successful(result) )
    {
        anvil_assert(is_vk_call_successful(presentation_results[0]));

        /* Return the most important error code reported */
        switch (presentation_results[0])
        {
            case VK_ERROR_DEVICE_LOST:
            {
                result = VK_ERROR_DEVICE_LOST;

                break;
            }

            case VK_ERROR_SURFACE_LOST_KHR:
            {
                if (result != VK_ERROR_DEVICE_LOST)
                {
                    result = VK_ERROR_SURFACE_LOST_KHR;
                }

                break;
            }

            case VK_ERROR_OUT_OF_DATE_KHR:
            {
                if (result != VK_ERROR_DEVICE_LOST      &&
                    result != VK_ERROR_SURFACE_LOST_KHR)
                {
                    result = VK_ERROR_OUT_OF_DATE_KHR;
                }

                break;
            }

            case VK_SUBOPTIMAL_KHR:
            {
                if (result != VK_ERROR_DEVICE_LOST      &&
                    result != VK_ERROR_SURFACE_LOST_KHR &&
                    result != VK_ERROR_OUT_OF_DATE_KHR)
                {
                    result = VK_SUBOPTIMAL_KHR;
                }

                break;
            }

            default:
            {
                anvil_assert(presentation_results[0] == VK_SUCCESS);
            }
        }
    }

end:
    return result;
}

/** Please see header for specification */
void Anvil::Queue::submit_command_buffers(uint32_t                                         n_command_buffers,
                                          std::shared_ptr<Anvil::CommandBufferBase> const* opt_cmd_buffer_ptrs,
                                          uint32_t                                         n_semaphores_to_signal,
                                          std::shared_ptr<Anvil::Semaphore> const*         opt_semaphore_to_signal_ptr_ptrs,
                                          uint32_t                                         n_semaphores_to_wait_on,
                                          std::shared_ptr<Anvil::Semaphore> const*         opt_semaphore_to_wait_on_ptr_ptrs,
                                          const VkPipelineStageFlags*                      opt_dst_stage_masks_to_wait_on_ptrs,
                                          bool                                             should_block,
                                          std::shared_ptr<Anvil::Fence>                    opt_fence_ptr)
{
    VkResult        result     (VK_ERROR_INITIALIZATION_FAILED);
    VkSubmitInfo    submit_info;

    VkCommandBuffer cmd_buffers_vk      [64];
    VkSemaphore     signal_semaphores_vk[64];
    VkSemaphore     wait_semaphores_vk  [64];

    ANVIL_REDUNDANT_VARIABLE(result);

    /* Sanity checks */
    anvil_assert(m_device_ptr.lock()->get_type() == Anvil::DEVICE_TYPE_SINGLE_GPU);
    anvil_assert(n_command_buffers               <  sizeof(cmd_buffers_vk)       / sizeof(cmd_buffers_vk      [0]) );
    anvil_assert(n_semaphores_to_signal          <  sizeof(signal_semaphores_vk) / sizeof(signal_semaphores_vk[0]) );
    anvil_assert(n_semaphores_to_wait_on         <  sizeof(wait_semaphores_vk)   / sizeof(wait_semaphores_vk  [0]) );

    /* Prepare for the submission */
    if (opt_fence_ptr == nullptr &&
        should_block)
    {
        opt_fence_ptr = Anvil::Fence::create(m_device_ptr,
                                             false /* create_signalled */);
    }

    for (uint32_t n_command_buffer = 0;
                  n_command_buffer < n_command_buffers;
                ++n_command_buffer)
    {
        cmd_buffers_vk[n_command_buffer] = opt_cmd_buffer_ptrs[n_command_buffer]->get_command_buffer();
    }

    for (uint32_t n_signal_semaphore = 0;
                  n_signal_semaphore < n_semaphores_to_signal;
                ++n_signal_semaphore)
    {
        signal_semaphores_vk[n_signal_semaphore] = opt_semaphore_to_signal_ptr_ptrs[n_signal_semaphore]->get_semaphore();
    }

    for (uint32_t n_wait_semaphore = 0;
                  n_wait_semaphore < n_semaphores_to_wait_on;
                ++n_wait_semaphore)
    {
        wait_semaphores_vk[n_wait_semaphore] = opt_semaphore_to_wait_on_ptr_ptrs[n_wait_semaphore]->get_semaphore();
    }

    submit_info.commandBufferCount   = n_command_buffers;
    submit_info.pCommandBuffers      = cmd_buffers_vk;
    submit_info.pNext                = nullptr;
    submit_info.pSignalSemaphores    = signal_semaphores_vk;
    submit_info.pWaitDstStageMask    = opt_dst_stage_masks_to_wait_on_ptrs;
    submit_info.pWaitSemaphores      = wait_semaphores_vk;
    submit_info.signalSemaphoreCount = n_semaphores_to_signal;
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = n_semaphores_to_wait_on;

    /* Go for it */
    result = vkQueueSubmit(m_queue,
                           1, /* submitCount */
                          &submit_info,
                          (opt_fence_ptr != nullptr) ? opt_fence_ptr->get_fence() 
                                                     : VK_NULL_HANDLE);
    anvil_assert_vk_call_succeeded(result);

    if (should_block)
    {
        /* Wait till initialization finishes GPU-side */
        std::shared_ptr<Anvil::SGPUDevice> sgpu_device_locked_ptr(std::dynamic_pointer_cast<Anvil::SGPUDevice>(m_device_ptr.lock() ) );

        result = vkWaitForFences(sgpu_device_locked_ptr->get_device_vk(),
                                 1, /* fenceCount */
                                 opt_fence_ptr->get_fence_ptr(),
                                 VK_TRUE,     /* waitAll */
                                 UINT64_MAX); /* timeout */
        anvil_assert_vk_call_succeeded(result);
    }
}