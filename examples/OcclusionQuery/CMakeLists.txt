cmake_minimum_required(VERSION 2.8)
project (OcclusionQuery)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    include(CheckCXXCompilerFlag)
    
    CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
    CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
    
    if(COMPILER_SUPPORTS_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    elseif(COMPILER_SUPPORTS_CXX0X)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
    endif()
endif()

add_subdirectory   (../.. "${CMAKE_CURRENT_BINARY_DIR}/anvil")


include_directories(${Anvil_SOURCE_DIR}/include
                    ${OcclusionQuery_SOURCE_DIR}/include)

# Include the Vulkan header.
if (WIN32)
    include_directories($ENV{VK_SDK_PATH}/Include
                        $ENV{VULKAN_SDK}/Include)
    
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            link_directories   ($ENV{VK_SDK_PATH}/Bin
                                $ENV{VULKAN_SDK}/Bin)
    else()
            link_directories   ($ENV{VK_SDK_PATH}/Bin32
                                $ENV{VULKAN_SDK}/Bin32)
    endif()
else()
    include_directories($ENV{VK_SDK_PATH}/x86_64/include
                        $ENV{VULKAN_SDK}/x86_64/include
                        $ENV{VULKAN_SDK}/include)
    link_directories   ($ENV{VK_SDK_PATH}/x86_64/lib
                        $ENV{VULKAN_SDK}/x86_64/lib
                        $ENV{VULKAN_SDK}/lib)
endif()

# Create the OcclusionQuery project.
add_executable (OcclusionQuery include/app.h
                                        include/app.h
                                            src/app.cpp)

# Add linking dependencies for the example projects
add_dependencies     (OcclusionQuery Anvil)

if (WIN32)
    target_link_libraries(OcclusionQuery Anvil)
else()
    target_link_libraries(OcclusionQuery Anvil dl)
endif()