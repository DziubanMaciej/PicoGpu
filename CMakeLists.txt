cmake_minimum_required(VERSION 3.10)
project(PicoGpu CXX)
find_package(SystemCLanguage CONFIG REQUIRED)
include(CMakeMacros.cmake)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Define ISA assembler code generation
set(ASM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gpu/isa/assembler")
add_custom_command(
    OUTPUT "${ASM_DIR}/generated/parser.cpp" "${ASM_DIR}/generated/inc.h"
    COMMAND bison ${ASM_DIR}/parser.y --output=${ASM_DIR}/generated/parser.cpp --header=${ASM_DIR}/generated/inc.h --graph=${ASM_DIR}/generated/graph.dot
    DEPENDS ${ASM_DIR}/parser.y
    COMMENT "Generating ISA assembler parser"
)
add_custom_command(
    OUTPUT "${ASM_DIR}/generated/scanner.cpp"
    COMMAND flex --outfile=${ASM_DIR}/generated/scanner.cpp ${ASM_DIR}/scanner.l
    DEPENDS ${ASM_DIR}/scanner.l
    COMMENT "Generating ISA assembler scanner"
)
add_custom_target(GenerateAssembler ALL
    DEPENDS "${ASM_DIR}/generated/parser.cpp" "${ASM_DIR}/generated/inc.h" "${ASM_DIR}/generated/scanner.cpp"
)
list(APPEND GPU_SOURCES "${ASM_DIR}/generated/parser.cpp" "${ASM_DIR}/generated/scanner.cpp")

# Define GPU library
append_sources(GPU_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu" OFF)
add_library(Gpu STATIC ${GPU_SOURCES})
target_link_libraries(Gpu PUBLIC SystemC::systemc)
target_include_directories(Gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET Gpu PROPERTY CXX_STANDARD ${SystemC_CXX_STANDARD})
add_dependencies(Gpu GenerateAssembler)

# Define GPU tests
function (enable_gpu_test TEST_NAME TARGET_NAME)
    add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TARGET_NAME}> ${ARGN})
    set_tests_properties(${TEST_NAME} PROPERTIES ENVIRONMENT "SC_COPYRIGHT_MESSAGE=DISABLE")
endfunction()
function (define_gpu_test TARGET_NAME)
    set(OPTIONS DONT_ENABLE)
    set(MULTI_VALUE_ARGS SOURCES CMDLINE)
    cmake_parse_arguments(ARGS "${OPTIONS}" "" "${MULTI_VALUE_ARGS}" ${ARGN} )

    add_executable(${TARGET_NAME} ${ARGS_SOURCES})
    target_link_libraries(${TARGET_NAME} PRIVATE Gpu)
    set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD ${SystemC_CXX_STANDARD})
    if (NOT ${ARGS_DONT_ENABLE})
        enable_gpu_test(${TARGET_NAME} ${TARGET_NAME} ${ARGS_CMDLINE})
    endif()
    target_compile_definitions(${TARGET_NAME} PRIVATE TEST_NAME="${TARGET_NAME}")
endfunction()

enable_testing()
define_gpu_test(MemoryControllerTest SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/memory_controller_test.cpp")
define_gpu_test(GpuTest SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/gpu_test.cpp")
define_gpu_test(UserBlitterTest DONT_ENABLE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/user_blitter_test.cpp")
    enable_gpu_test(UserBlitterTestWithMemController    UserBlitterTest "1")
    enable_gpu_test(UserBlitterTestWithoutMemController UserBlitterTest "0")
define_gpu_test(ShaderUnitTest SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/shader_unit_test.cpp")
define_gpu_test(ShaderFrontendTest SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/shader_frontend_test.cpp")
define_gpu_test(AssemblerTest SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/gpu_tests/assembler_test.cpp")
