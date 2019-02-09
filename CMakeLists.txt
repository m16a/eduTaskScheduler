cmake_minimum_required(VERSION 2.8.9)
project(task_sheduller)

set (CMAKE_CXX_STANDARD 11)
#Bring the headers, such as Student.h into the project

#Can manually add the sources using the set command as follows:
set(SOURCES main.cpp)

include_directories(${CMAKE_SOURCE_DIR}/3rd/include/)

message(INFO easy_profiler lib path ${CMAKE_SOURCE_DIR}/3rd/)
find_library(EASY_PROFILER_LIBRARY easy_profiler HINT ${CMAKE_SOURCE_DIR}/3rd/)

add_executable(task_sheduller ${SOURCES})

target_link_libraries(task_sheduller ${EASY_PROFILER_LIBRARY} pthread)
