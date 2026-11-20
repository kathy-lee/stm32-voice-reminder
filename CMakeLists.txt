# Host build: compiles the portable firmware modules for the PC and runs
# their tests. The STM32 firmware itself is built from the CubeMX project.
cmake_minimum_required(VERSION 3.20)
project(voice_reminder_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

add_library(ima_adpcm STATIC codec/ima_adpcm.c)
target_include_directories(ima_adpcm PUBLIC codec)

enable_testing()
add_executable(test_ima_adpcm tests/host/test_ima_adpcm.c)
target_link_libraries(test_ima_adpcm PRIVATE ima_adpcm)
target_compile_definitions(test_ima_adpcm PRIVATE
    VECTOR_DIR="${CMAKE_SOURCE_DIR}/tests/vectors")
add_test(NAME ima_adpcm COMMAND test_ima_adpcm)
