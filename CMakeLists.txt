cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Tetris)
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SRCS EXCLUDE REGEX ".*/raylib_test\\.cpp$")

add_subdirectory(vendor/raylib)

add_executable(${PROJECT_NAME} ${SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    vendor
    vendor/raylib/src
    vendor/json/single_include
)
target_link_libraries(${PROJECT_NAME} raylib)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

# Copy the font and other assets next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
