cmake_minimum_required(VERSION 3.10)
project(MessSystemC)
find_package(SystemCLanguage CONFIG REQUIRED)
include(CMakeMacros.cmake)

# Define GPU library
append_sources(GPU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu" OFF)
add_library(Gpu STATIC ${GPU_SOURCES})
target_link_libraries(Gpu PUBLIC SystemC::systemc)
target_include_directories(Gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET Gpu PROPERTY CXX_STANDARD ${SystemC_CXX_STANDARD})

# Define GPU tests
append_sources(GPU_TESTS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests" OFF)
add_executable(GpuTests ${GPU_TESTS_SOURCES})
target_link_libraries(GpuTests PRIVATE Gpu)
set_property(TARGET GpuTests PROPERTY CXX_STANDARD ${SystemC_CXX_STANDARD})
