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
#include "wrappers/device.h"
#include "wrappers/physical_device.h"
#include "wrappers/sampler.h"

/** Please see header for specification */
Anvil::Sampler::Sampler(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                        VkFilter                         mag_filter,
                        VkFilter                         min_filter,
                        VkSamplerMipmapMode              mipmap_mode,
                        VkSamplerAddressMode             address_mode_u,
                        VkSamplerAddressMode             address_mode_v,
                        VkSamplerAddressMode             address_mode_w,
                        float                            lod_bias,
                        float                            max_anisotropy,
                        bool                             compare_enable,
                        VkCompareOp                      compare_op,
                        float                            min_lod,
                        float                            max_lod,
                        VkBorderColor                    border_color,
                        bool                             use_unnormalized_coordinates)
    :m_address_mode_u              (address_mode_u),
     m_address_mode_v              (address_mode_v),
     m_address_mode_w              (address_mode_w),
     m_border_color                (border_color),
     m_compare_enable              (compare_enable),
     m_compare_op                  (compare_op),
     m_device_ptr                  (device_ptr),
     m_lod_bias                    (lod_bias),
     m_mag_filter                  (mag_filter),
     m_max_anisotropy              (max_anisotropy),
     m_max_lod                     (max_lod),
     m_min_filter                  (min_filter),
     m_min_lod                     (min_lod),
     m_mipmap_mode                 (mipmap_mode),
     m_sampler                     (VK_NULL_HANDLE),
     m_use_unnormalized_coordinates(use_unnormalized_coordinates)
{
    std::shared_ptr<Anvil::BaseDevice> device_locked_ptr  (m_device_ptr);
    VkSamplerCreateInfo                sampler_create_info;
    VkResult                           result             (VK_ERROR_INITIALIZATION_FAILED);

    ANVIL_REDUNDANT_VARIABLE(result);

    /* Spawn a new sampler */
    sampler_create_info.addressModeU            = address_mode_u;
    sampler_create_info.addressModeV            = address_mode_v;
    sampler_create_info.addressModeW            = address_mode_w;
    sampler_create_info.anisotropyEnable        = static_cast<VkBool32>((max_anisotropy > 1.0f) ? VK_TRUE : VK_FALSE);
    sampler_create_info.borderColor             = border_color;
    sampler_create_info.compareEnable           = static_cast<VkBool32>(compare_enable ? VK_TRUE : VK_FALSE);
    sampler_create_info.compareOp               = compare_op;
    sampler_create_info.flags                   = 0;
    sampler_create_info.magFilter               = mag_filter;
    sampler_create_info.maxAnisotropy           = max_anisotropy;
    sampler_create_info.maxLod                  = max_lod;
    sampler_create_info.minFilter               = min_filter;
    sampler_create_info.minLod                  = min_lod;
    sampler_create_info.mipLodBias              = lod_bias;
    sampler_create_info.mipmapMode              = mipmap_mode;
    sampler_create_info.pNext                   = nullptr;
    sampler_create_info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.unnormalizedCoordinates = static_cast<VkBool32>(use_unnormalized_coordinates ? VK_TRUE : VK_FALSE);

    result = vkCreateSampler(device_locked_ptr->get_device_vk(),
                            &sampler_create_info,
                             nullptr, /* pAllocator */
                            &m_sampler);
    anvil_assert_vk_call_succeeded(result);

    /* Register the event instance */
    Anvil::ObjectTracker::get()->register_object(Anvil::OBJECT_TYPE_SAMPLER,
                                                  this);
}

/* Please see header for specification */
Anvil::Sampler::~Sampler()
{
    if (m_sampler != VK_NULL_HANDLE)
    {
        std::shared_ptr<Anvil::BaseDevice> device_locked_ptr(m_device_ptr);

        vkDestroySampler(device_locked_ptr->get_device_vk(),
                         m_sampler,
                         nullptr /* pAllocator */);

        m_sampler = VK_NULL_HANDLE;
    }

    Anvil::ObjectTracker::get()->unregister_object(Anvil::OBJECT_TYPE_SAMPLER,
                                                    this);
}

/* Please see header for specification */
std::shared_ptr<Anvil::Sampler> Anvil::Sampler::create(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                                                       VkFilter                         mag_filter,
                                                       VkFilter                         min_filter,
                                                       VkSamplerMipmapMode              mipmap_mode,
                                                       VkSamplerAddressMode             address_mode_u,
                                                       VkSamplerAddressMode             address_mode_v,
                                                       VkSamplerAddressMode             address_mode_w,
                                                       float                            lod_bias,
                                                       float                            max_anisotropy,
                                                       bool                             compare_enable,
                                                       VkCompareOp                      compare_op,
                                                       float                            min_lod,
                                                       float                            max_lod,
                                                       VkBorderColor                    border_color,
                                                       bool                             use_unnormalized_coordinates)
{
    std::shared_ptr<Anvil::BaseDevice> device_locked_ptr(device_ptr);
    std::shared_ptr<Anvil::Sampler>    result_ptr;

    result_ptr.reset(
        new Anvil::Sampler(device_ptr,
                           mag_filter,
                           min_filter,
                           mipmap_mode,
                           address_mode_u,
                           address_mode_v,
                           address_mode_w,
                           lod_bias,
                           max_anisotropy,
                           compare_enable,
                           compare_op,
                           min_lod,
                           max_lod,
                           border_color,
                           use_unnormalized_coordinates)
    );

    return result_ptr;
}