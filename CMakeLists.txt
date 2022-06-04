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
enable_testing()
function (define_gpu_test TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE Gpu)
    set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD ${SystemC_CXX_STANDARD})
    add_test(NAME ${TARGET_NAME} COMMAND $<TARGET_FILE:${TARGET_NAME}>)
endfunction()
define_gpu_test(MemoryControllerTest "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/memory_controller_test.cpp")
define_gpu_test(GpuTest "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/gpu_test.cpp")
