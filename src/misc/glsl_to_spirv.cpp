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

#include "config.h"
#include "misc/glsl_to_spirv.h"
#include "misc/io.h"
#include "wrappers/device.h"
#include <algorithm>
#include <sstream>

#ifndef _WIN32
    #include <limits.h>
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#ifdef ANVIL_LINK_WITH_GLSLANG
    #undef snprintf

    #if defined(_MSC_VER)
        #pragma warning(push)
        #pragma warning(disable: 4100)
        #pragma warning(disable: 4365)
        #pragma warning(disable: 4625)
    #endif

    #include "glslang/SPIRV/GlslangToSpv.h"

    #ifdef _MSC_VER
        #pragma warning(pop)
    #endif
#endif

#ifndef ANVIL_LINK_WITH_GLSLANG
    #define SPIRV_FILE_NAME_LEN 100
#else
    /* Helper class used as a process-wide RAII (de-)-initializer for glslangvalidator. **/
    class GLSLangGlobalInitializer
    {
    public:
        /* Constructor. */
        GLSLangGlobalInitializer()
        {
            glslang::InitializeProcess();
        }

        /* Destructor */
        ~GLSLangGlobalInitializer()
        {
            glslang::FinalizeProcess();
        }
    };

    /* Constructor. */
    Anvil::GLSLangLimits::GLSLangLimits(std::weak_ptr<Anvil::BaseDevice> device_ptr)
    {
        const VkPhysicalDeviceLimits& limits                         = device_ptr.lock()->get_physical_device_properties().limits;
        VkSampleCountFlags            max_sampled_image_sample_count;
        int32_t                       max_sampled_image_samples      = 0;
        VkSampleCountFlags            max_storage_image_sample_count = limits.storageImageSampleCounts;
        int32_t                       max_storage_image_samples      = 0;

        max_sampled_image_sample_count = std::max<VkSampleCountFlags>(limits.sampledImageColorSampleCounts, limits.sampledImageDepthSampleCounts);
        max_sampled_image_sample_count = std::max<VkSampleCountFlags>(max_sampled_image_sample_count,       limits.sampledImageIntegerSampleCounts);
        max_sampled_image_sample_count = std::max<VkSampleCountFlags>(max_sampled_image_sample_count,       limits.sampledImageStencilSampleCounts);

        const struct SampleCountToSamplesData
        {
            VkSampleCountFlags sample_count;
            int32_t*           out_result_ptr;
        } conversion_items[] =
        {
            {max_sampled_image_sample_count, &max_sampled_image_samples},
            {max_storage_image_sample_count, &max_storage_image_samples}
        };
        const uint32_t n_conversion_items = sizeof(conversion_items) / sizeof(conversion_items[0]);

        for (uint32_t n_conversion_item = 0;
                      n_conversion_item < n_conversion_items;
                    ++n_conversion_item)
        {
            const SampleCountToSamplesData& current_item = conversion_items[n_conversion_item];
            int32_t                         result       = 1;

            if (current_item.sample_count & VK_SAMPLE_COUNT_16_BIT)
            {
                result = 16;
            }
            else
            if (current_item.sample_count & VK_SAMPLE_COUNT_8_BIT)
            {
                result = 8;
            }
            else
            if (current_item.sample_count & VK_SAMPLE_COUNT_4_BIT)
            {
                result = 4;
            }
            else
            if (current_item.sample_count & VK_SAMPLE_COUNT_2_BIT)
            {
                result = 2;
            }

            *current_item.out_result_ptr = result;
        }

        #define CLAMP_TO_INT_MAX(x) ((x <= INT_MAX) ? x : INT_MAX)

        m_resources_ptr.reset(
            new TBuiltInResource()
        );

        m_resources_ptr->maxLights                                   = 32; /* irrelevant to Vulkan */
        m_resources_ptr->maxClipPlanes                               = 6;  /* irrelevant to Vulkan */
        m_resources_ptr->maxTextureUnits                             = 32; /* irrelevant to Vulkan */
        m_resources_ptr->maxTextureCoords                            = 32; /* irrelevant to Vulkan */
        m_resources_ptr->maxVertexAttribs                            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxVertexInputAttributes) );
        m_resources_ptr->maxVertexUniformComponents                  = 4096; /* irrelevant to Vulkan  */
        m_resources_ptr->maxVaryingFloats                            = 64;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxVertexTextureImageUnits                  = 32;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxCombinedTextureImageUnits                = 80;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxTextureImageUnits                        = 32;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxFragmentUniformComponents                = 4096; /* irrelevant to Vulkan? */
        m_resources_ptr->maxDrawBuffers                              = 32;   /* irrelevant to Vulkan  */
        m_resources_ptr->maxVertexUniformVectors                     = 128;  /* irrelevant to Vulkan? */
        m_resources_ptr->maxVaryingVectors                           = 8;    /* irrelevant to Vulkan? */
        m_resources_ptr->maxFragmentUniformVectors                   = 16;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxVertexOutputVectors                      = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxVertexOutputComponents  / 4) );
        m_resources_ptr->maxFragmentInputVectors                     = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxFragmentInputComponents / 4) );
        m_resources_ptr->minProgramTexelOffset                       = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.minTexelOffset) );
        m_resources_ptr->maxProgramTexelOffset                       = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTexelOffset) );
        m_resources_ptr->maxClipDistances                            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxClipDistances) );
        m_resources_ptr->maxComputeWorkGroupCountX                   = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupCount[0]) );
        m_resources_ptr->maxComputeWorkGroupCountY                   = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupCount[1]) );
        m_resources_ptr->maxComputeWorkGroupCountZ                   = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupCount[2]) );
        m_resources_ptr->maxComputeWorkGroupSizeX                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupSize[0]) );
        m_resources_ptr->maxComputeWorkGroupSizeY                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupSize[1]) );
        m_resources_ptr->maxComputeWorkGroupSizeZ                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxComputeWorkGroupSize[2]) );
        m_resources_ptr->maxComputeUniformComponents                 = 1024; /* irrelevant to Vulkan? */
        m_resources_ptr->maxComputeTextureImageUnits                 = 16;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxComputeImageUniforms                     = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxComputeAtomicCounters                    = 8;    /* irrelevant to Vulkan */
        m_resources_ptr->maxComputeAtomicCounterBuffers              = 1;    /* irrelevant to Vulkan */
        m_resources_ptr->maxVaryingComponents                        = 60;   /* irrelevant to Vulkan */
        m_resources_ptr->maxVertexOutputComponents                   = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxVertexOutputComponents) );
        m_resources_ptr->maxGeometryInputComponents                  = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxGeometryInputComponents) );
        m_resources_ptr->maxGeometryOutputComponents                 = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxGeometryOutputComponents) );
        m_resources_ptr->maxFragmentInputComponents                  = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxFragmentInputComponents) );
        m_resources_ptr->maxImageUnits                               = 8; /* irrelevant to Vulkan */
        m_resources_ptr->maxCombinedImageUnitsAndFragmentOutputs     = 8; /* irrelevant to Vulkan? */
        m_resources_ptr->maxCombinedShaderOutputResources            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxFragmentCombinedOutputResources) );
        m_resources_ptr->maxImageSamples                             = max_storage_image_samples;
        m_resources_ptr->maxVertexImageUniforms                      = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxTessControlImageUniforms                 = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxTessEvaluationImageUniforms              = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxGeometryImageUniforms                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxFragmentImageUniforms                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxCombinedImageUniforms                    = static_cast<int32_t>(CLAMP_TO_INT_MAX(5 /* vs, tc, te, gs, fs */ * limits.maxPerStageDescriptorStorageImages) );
        m_resources_ptr->maxGeometryTextureImageUnits                = 16; /* irrelevant to Vulkan? */
        m_resources_ptr->maxGeometryOutputVertices                   = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxGeometryOutputVertices) );
        m_resources_ptr->maxGeometryTotalOutputComponents            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxGeometryTotalOutputComponents) );
        m_resources_ptr->maxGeometryUniformComponents                = 1024; /* irrelevant to Vulkan? */
        m_resources_ptr->maxGeometryVaryingComponents                = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxGeometryInputComponents) );
        m_resources_ptr->maxTessControlInputComponents               = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationControlPerVertexInputComponents) );
        m_resources_ptr->maxTessControlOutputComponents              = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationControlPerVertexOutputComponents) );
        m_resources_ptr->maxTessControlTextureImageUnits             = 16;   /* irrelevant to Vulkan? */
        m_resources_ptr->maxTessControlUniformComponents             = 1024; /* irrelevant to Vulkan? */
        m_resources_ptr->maxTessControlTotalOutputComponents         = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationControlTotalOutputComponents) );
        m_resources_ptr->maxTessEvaluationInputComponents            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationEvaluationInputComponents) );
        m_resources_ptr->maxTessEvaluationOutputComponents           = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationEvaluationOutputComponents) );
        m_resources_ptr->maxTessEvaluationTextureImageUnits          = 16; /* irrelevant to Vulkan? */
        m_resources_ptr->maxTessEvaluationUniformComponents          = 1024; /* irrelevant to Vulkan? */
        m_resources_ptr->maxTessPatchComponents                      = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationControlPerPatchOutputComponents) );
        m_resources_ptr->maxPatchVertices                            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationPatchSize) );
        m_resources_ptr->maxTessGenLevel                             = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxTessellationGenerationLevel) );
        m_resources_ptr->maxViewports                                = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxViewports) );
        m_resources_ptr->maxVertexAtomicCounters                     = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTessControlAtomicCounters                = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTessEvaluationAtomicCounters             = 0; /* not supported in Vulkan */
        m_resources_ptr->maxGeometryAtomicCounters                   = 0; /* not supported in Vulkan */
        m_resources_ptr->maxFragmentAtomicCounters                   = 0; /* not supported in Vulkan */
        m_resources_ptr->maxCombinedAtomicCounters                   = 0; /* not supported in Vulkan */
        m_resources_ptr->maxAtomicCounterBindings                    = 0; /* not supported in Vulkan */
        m_resources_ptr->maxVertexAtomicCounterBuffers               = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTessControlAtomicCounterBuffers          = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTessEvaluationAtomicCounterBuffers       = 0; /* not supported in Vulkan */
        m_resources_ptr->maxGeometryAtomicCounterBuffers             = 0; /* not supported in Vulkan */
        m_resources_ptr->maxFragmentAtomicCounterBuffers             = 0; /* not supported in Vulkan */
        m_resources_ptr->maxCombinedAtomicCounterBuffers             = 0; /* not supported in Vulkan */
        m_resources_ptr->maxAtomicCounterBufferSize                  = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTransformFeedbackBuffers                 = 0; /* not supported in Vulkan */
        m_resources_ptr->maxTransformFeedbackInterleavedComponents   = 0; /* not supported in Vulkan */
        m_resources_ptr->maxCullDistances                            = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxCullDistances) );
        m_resources_ptr->maxCombinedClipAndCullDistances             = static_cast<int32_t>(CLAMP_TO_INT_MAX(limits.maxCombinedClipAndCullDistances) );
        m_resources_ptr->maxSamples                                  = (max_sampled_image_samples > max_storage_image_samples) ? CLAMP_TO_INT_MAX(max_sampled_image_samples)
                                                                                                                               : CLAMP_TO_INT_MAX(max_storage_image_samples);
        m_resources_ptr->limits.nonInductiveForLoops                 = 1;
        m_resources_ptr->limits.whileLoops                           = 1;
        m_resources_ptr->limits.doWhileLoops                         = 1;
        m_resources_ptr->limits.generalUniformIndexing               = 1;
        m_resources_ptr->limits.generalAttributeMatrixVectorIndexing = 1;
        m_resources_ptr->limits.generalVaryingIndexing               = 1;
        m_resources_ptr->limits.generalSamplerIndexing               = 1;
        m_resources_ptr->limits.generalVariableIndexing              = 1;
        m_resources_ptr->limits.generalConstantMatrixVectorIndexing  = 1;
    }

    static GLSLangGlobalInitializer glslang_helper;
#endif


/* Please see header for specification */
Anvil::GLSLShaderToSPIRVGenerator::GLSLShaderToSPIRVGenerator(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                                                              const Mode&                      mode,
                                                              std::string                      data,
                                                              ShaderStage                      shader_stage)
    :m_data           (data),
#ifdef ANVIL_LINK_WITH_GLSLANG
     m_limits         (device_ptr),
#endif
     m_mode           (mode),
     m_shader_stage   (shader_stage),
     m_spirv_blob     (nullptr),
     m_spirv_blob_size(0)
{
    /* Stub */
}

/* Please see header for specification */
Anvil::GLSLShaderToSPIRVGenerator::~GLSLShaderToSPIRVGenerator()
{
    if (m_spirv_blob != nullptr)
    {
        delete [] m_spirv_blob;

        m_spirv_blob = nullptr;
    }
}

/* Please see header for specification */
bool Anvil::GLSLShaderToSPIRVGenerator::add_empty_definition(std::string definition_name)
{
    return add_definition_value_pair(definition_name,
                                     "");
}

/* Please see header for specification */
bool Anvil::GLSLShaderToSPIRVGenerator::add_extension_behavior(std::string       extension_name,
                                                               ExtensionBehavior behavior)
{
    bool result = false;

    if (m_extension_behaviors.find(extension_name) != m_extension_behaviors.end() )
    {
        anvil_assert(false);

        goto end;
    }

    m_extension_behaviors[extension_name] = behavior;

    /* All done */
    result = true;
end:
    return result;
}

/* Please see header for specification */
bool Anvil::GLSLShaderToSPIRVGenerator::add_definition_value_pair(std::string definition_name,
                                                                  std::string value)
{
    bool result = false;

    if (m_definition_values.find(definition_name) != m_definition_values.end() )
    {
        anvil_assert(false);

        goto end;
    }

    m_definition_values[definition_name] = value;

    /* All done */
    result = true;
end:
    return result;
}

/* Please see header for specification */
bool Anvil::GLSLShaderToSPIRVGenerator::bake_spirv_blob()
{
    std::string    final_glsl_source_string;
    bool           glsl_filename_is_temporary = false;
    std::string    glsl_filename_with_path;
    const uint32_t n_extension_behaviors      = static_cast<uint32_t>(m_extension_behaviors.size() );
    const uint32_t n_definition_values        = static_cast<uint32_t>(m_definition_values.size() );
    bool           result                     = false;

    ANVIL_REDUNDANT_VARIABLE(glsl_filename_is_temporary);

    {
        switch (m_mode)
        {
            case MODE_LOAD_SOURCE_FROM_FILE:
            {
                char* glsl_source = nullptr;

                Anvil::IO::read_file(m_data,
                                     true, /* is_text_file */
                                    &glsl_source,
                                     nullptr);  /* out_opt_size_ptr */

                if (glsl_source == nullptr)
                {
                    anvil_assert(glsl_source != nullptr);

                    goto end;
                }

                final_glsl_source_string = std::string(glsl_source);

                delete [] glsl_source;
                break;
            }

            case MODE_USE_SPECIFIED_SOURCE:
            {
                final_glsl_source_string = m_data;

                break;
            }

            default:
            {
                /* Unrecognized mode specified for a GLSLShaderToSPIRVGenerator instance. */
                anvil_assert(false);
            }
        }
    }

    if (n_extension_behaviors > 0 ||
        n_definition_values   > 0)
    {
        size_t glsl_source_string_second_line_index;

        /* Inject extension behavior definitions, starting from the second line. According to the spec, first line in
         * a GLSL shader must define the ESSL/GLSL version, and glslangvalidator seems to be pretty
         * strict about this. */
        glsl_source_string_second_line_index = final_glsl_source_string.find_first_of('\n') + 1;

        for (auto map_iterator  = m_extension_behaviors.begin();
                  map_iterator != m_extension_behaviors.end();
                ++map_iterator)
        {
            const ExtensionBehavior& current_extension_behavior      = map_iterator->second;
            std::string              current_extension_behavior_glsl = get_extension_behavior_glsl_code(current_extension_behavior);
            std::string              current_extension_name          = map_iterator->first;
            std::string              new_line                        = std::string("#extension ")      +
                                                                       current_extension_name          +
                                                                       std::string(" : ")              +
                                                                       current_extension_behavior_glsl +
                                                                       "\n";

            final_glsl_source_string.insert(glsl_source_string_second_line_index,
                                            new_line);

            glsl_source_string_second_line_index += new_line.length();
        }

        /* Follow with #defines which associate values with definition names */
        for (auto map_iterator  = m_definition_values.begin();
                  map_iterator != m_definition_values.end();
                ++map_iterator)
        {
            std::string current_key   = map_iterator->first;
            std::string current_value = map_iterator->second;
            std::string new_line      = std::string("#define ") + current_key + std::string(" ") + current_value + "\n";

            final_glsl_source_string.insert(glsl_source_string_second_line_index,
                                            new_line);
        }
    }

    if (m_mode == MODE_LOAD_SOURCE_FROM_FILE)
    {
        glsl_filename_is_temporary = false;
        glsl_filename_with_path    = m_data;
    }

    /* Form a temporary file name we will use to write the modified GLSL shader to. */
    #ifndef ANVIL_LINK_WITH_GLSLANG
    {
        switch (m_shader_stage)
        {
            case SHADER_STAGE_COMPUTE:                 glsl_filename_with_path = "temp.comp"; break;
            case SHADER_STAGE_FRAGMENT:                glsl_filename_with_path = "temp.frag"; break;
            case SHADER_STAGE_GEOMETRY:                glsl_filename_with_path = "temp.geom"; break;
            case SHADER_STAGE_TESSELLATION_CONTROL:    glsl_filename_with_path = "temp.tesc"; break;
            case SHADER_STAGE_TESSELLATION_EVALUATION: glsl_filename_with_path = "temp.tese"; break;
            case SHADER_STAGE_VERTEX:                  glsl_filename_with_path = "temp.vert"; break;

            default:
            {
                anvil_assert(false);

                goto end;
            }
        }

        /* Write down the file to a temporary location */
        Anvil::IO::write_text_file(glsl_filename_with_path,
                                   final_glsl_source_string);

        glsl_filename_is_temporary = true;
    }
    #endif


    #ifdef ANVIL_LINK_WITH_GLSLANG
    {
        result = bake_spirv_blob_by_calling_glslang(final_glsl_source_string.c_str() );
    }
    #else
    {
        /* We need to point glslangvalidator at a location where it can stash the SPIR-V blob. */
        result = bake_spirv_blob_by_spawning_glslang_process(glsl_filename_with_path,
                                                             "temp.spv");
    }
    #endif

end:
    return result;
}

#ifdef ANVIL_LINK_WITH_GLSLANG
    /** Takes the GLSL source code, specified under @param body, converts it to SPIR-V and stores
     *  the blob data under m_spirv_blob & m_spirv_blob_size
     *
     *  @param body GLSL source code to use as input. Must not be nullptr.
     *
     *  @return true if successful, false otherwise.
     **/
    bool Anvil::GLSLShaderToSPIRVGenerator::bake_spirv_blob_by_calling_glslang(const char* body)
    {
        const EShLanguage         glslang_shader_stage = get_glslang_shader_stage();
        glslang::TIntermediate*   intermediate_ptr     = nullptr;
        glslang::TProgram*        new_program_ptr      = new glslang::TProgram();
        glslang::TShader*         new_shader_ptr       = new glslang::TShader(glslang_shader_stage);
        bool                      result               = false;
        std::vector<unsigned int> spirv_blob;

        anvil_assert(new_program_ptr != nullptr &&
                     new_shader_ptr != nullptr);

        if (new_program_ptr != nullptr &&
            new_shader_ptr  != nullptr)
        {
            /* Try to compile the shader */
            new_shader_ptr->setStrings(&body,
                                       1);

            result = new_shader_ptr->parse(m_limits.get_resource_ptr(),
                                           110,   /* defaultVersion    */
                                           false, /* forwardCompatible */
                                           (EShMessages) (EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules) );

            if (!result)
            {
                /* Shader compilation failed.. */
                fprintf(stderr,
                        "Shader compilation failed.");

                goto end;
            }

            /* Link the program */
            new_program_ptr->addShader(new_shader_ptr);

            if (!new_program_ptr->link(EShMsgDefault) )
            {
                /* Linking operation failed */
                fprintf(stderr,
                        "Program linking failed.");

                goto end;
            }

            /* Convert the intermediate representation to SPIR-V blob. */
            intermediate_ptr = new_program_ptr->getIntermediate(glslang_shader_stage);

            if (intermediate_ptr == nullptr)
            {
                anvil_assert(intermediate_ptr != nullptr);

                goto end;
            }

            glslang::GlslangToSpv(*intermediate_ptr,
                                  spirv_blob);

            if (spirv_blob.size() == 0)
            {
                anvil_assert(spirv_blob.size() != 0);

                goto end;
            }

            m_spirv_blob_size = static_cast<uint32_t>(spirv_blob.size() ) * sizeof(unsigned int);
            m_spirv_blob      = new char[m_spirv_blob_size];

            if (m_spirv_blob == nullptr)
            {
                anvil_assert(m_spirv_blob != nullptr);

                goto end;
            }

            memcpy(m_spirv_blob,
                  &spirv_blob[0],
                   m_spirv_blob_size);
        }

        /* All done */
        result = true;
    end:
        if (new_program_ptr != nullptr)
        {
            delete new_program_ptr;

            new_program_ptr = nullptr;
        }

        if (new_shader_ptr != nullptr)
        {
            delete new_shader_ptr;

            new_shader_ptr = nullptr;
        }

        return result;
    }

    /** Retrieves EShLanguage corresponding to m_shader_stage assigned to the GLSLShaderToSPIRVGenerator instance. **/
    EShLanguage Anvil::GLSLShaderToSPIRVGenerator::get_glslang_shader_stage() const
    {
        EShLanguage result = EShLangCount;

        switch (m_shader_stage)
        {
            case Anvil::SHADER_STAGE_COMPUTE:                 result = EShLangCompute;        break;
            case Anvil::SHADER_STAGE_FRAGMENT:                result = EShLangFragment;       break;
            case Anvil::SHADER_STAGE_GEOMETRY:                result = EShLangGeometry;       break;
            case Anvil::SHADER_STAGE_TESSELLATION_CONTROL:    result = EShLangTessControl;    break;
            case Anvil::SHADER_STAGE_TESSELLATION_EVALUATION: result = EShLangTessEvaluation; break;
            case Anvil::SHADER_STAGE_VERTEX:                  result = EShLangVertex;         break;

            default:
            {
                anvil_assert(false);
            }
        }

        return result;
    }
#else
    /** Reads contents of a file under location @param glsl_filename_with_path and treats the retrieved contents as GLSL source code,
     *  which is then used for GLSL->SPIRV conversion process. The result blob is stored at @param spirv_filename_with_path. The function
     *  then reads the blob contents and stores it under m_spirv_blob & m_spirv_blob_size
     *
     *  @param glsl_filename_with_path  As per description above. Must not be nullptr.
     *  @param spirv_filename_with_path As per description above. Must not be nullptr.
     *
     *  @return true if successful, false otherwise.
     **/
    bool Anvil::GLSLShaderToSPIRVGenerator::bake_spirv_blob_by_spawning_glslang_process(const std::string& glsl_filename_with_path,
                                                                                        const std::string& spirv_filename_with_path)
    {
        std::string glslangvalidator_params;
        bool        result                  = false;
        size_t      spirv_file_size         = 0;

        #ifdef _WIN32
        {
            /* Launch glslangvalidator and wait until it finishes doing the job */
            PROCESS_INFORMATION process_info;
            STARTUPINFO         startup_info;

            glslangvalidator_params = "dummy -V -o \"" + spirv_filename_with_path + "\" \"" + glsl_filename_with_path + "\"";

            memset(&process_info,
                   0,
                   sizeof(process_info) );
            memset(&startup_info,
                   0,
                   sizeof(startup_info) );

            startup_info.cb = sizeof(startup_info);

            if (!CreateProcess(".\\glslangValidator.exe",
                               (LPSTR) glslangvalidator_params.c_str(),
                               nullptr, /* lpProcessAttributes */
                               nullptr, /* lpThreadAttributes */
                               FALSE, /* bInheritHandles */
                               CREATE_NO_WINDOW, 
                               nullptr, /* lpEnvironment */
                               nullptr, /* lpCurrentDirectory */
                               &startup_info,
                               &process_info) )
            {
                anvil_assert(false);

                goto end;
            }

            /* Wait till glslangvalidator is done. */
            if (WaitForSingleObject(process_info.hProcess,
                                    INFINITE) != WAIT_OBJECT_0)
            {
                anvil_assert(false);

                goto end;
            }
        }
        #else
        {
            int32_t status;
            pid_t   child_pid = 0;

            child_pid = fork();

            if (child_pid == 0)
            {
                char* argv[6] = {(char*)"-S", (char*)"-V", (char*)"-o"};

                char  spirv_file_name[SPIRV_FILE_NAME_LEN];
                char  glsl_file_name[SPIRV_FILE_NAME_LEN];

                strcpy(spirv_file_name, spirv_filename_with_path.c_str());
                strcpy(glsl_file_name, glsl_filename_with_path.c_str());

                argv[3] = spirv_file_name;
                argv[4] = glsl_file_name;
                argv[5] = (char*)0;

                int32_t flag = execv("./glslangValidator", (char* const*)argv);
                if (flag == -1)
                {
                    anvil_assert(false);
                    goto end;
                }
            }
            else
            {
                do
                {
                    pid_t wpid = waitpid(child_pid, &status, WUNTRACED | WCONTINUED);
                    if (wpid == -1)
                    {
                        anvil_assert(false);
                        goto end;
                    }
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            }
        }
        #endif

        /* Now, read the SPIR-V file contents */
        Anvil::IO::read_file(spirv_filename_with_path.c_str(),
                             false, /* is_text_file */
                            &m_spirv_blob,
                            &spirv_file_size);

        if (m_spirv_blob == nullptr)
        {
            anvil_assert(m_spirv_blob != nullptr);

            goto end;
        }

        if (spirv_file_size <= 0)
        {
            anvil_assert(spirv_file_size > 0);

            goto end;
        }

        /* No need to keep the file any more. */
        Anvil::IO::delete_file(spirv_filename_with_path);

        m_spirv_blob_size = (uint32_t) spirv_file_size;
        result            = true;

    end:
        return result;
    }
#endif

/* Please see header for specification */
std::shared_ptr<Anvil::GLSLShaderToSPIRVGenerator> Anvil::GLSLShaderToSPIRVGenerator::create(std::weak_ptr<Anvil::BaseDevice> device_ptr,
                                                                                             const Mode&                      mode,
                                                                                             std::string                      data,
                                                                                             ShaderStage                      shader_stage)
{
    std::shared_ptr<Anvil::GLSLShaderToSPIRVGenerator> result_ptr;

    result_ptr.reset(
        new Anvil::GLSLShaderToSPIRVGenerator(device_ptr,
                                              mode,
                                              data,
                                              shader_stage)
    );

    return result_ptr;
}

/* Please see header for specification */
std::string Anvil::GLSLShaderToSPIRVGenerator::get_extension_behavior_glsl_code(const ExtensionBehavior& value) const
{
    std::string result;

    switch (value)
    {
        case EXTENSION_BEHAVIOR_DISABLE: result = "disable"; break;
        case EXTENSION_BEHAVIOR_ENABLE:  result = "enable";  break;
        case EXTENSION_BEHAVIOR_REQUIRE: result = "require"; break;
        case EXTENSION_BEHAVIOR_WARN:    result = "warn";    break;

        default:
        {
            anvil_assert(false);
        }
    }

    return result;
}