cmake_minimum_required(VERSION 3.22)
project(tnvk-jni)

find_library(vulkan-lib vulkan)
find_library(log-lib log)

add_library(
    tnvk-jni
    SHARED
    native-lib.cpp)

target_include_directories(tnvk-jni PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
    tnvk-jni
    ${vulkan-lib}
    ${log-lib})
