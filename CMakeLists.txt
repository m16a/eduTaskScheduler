cmake_minimum_required(VERSION 3.6.3)
project(task_sheduller)

set (CMAKE_CXX_STANDARD 11)

set(SOURCES main.cpp)

set (USE_PROFILER false)
if (${USE_PROFILER})
		include_directories(${CMAKE_SOURCE_DIR}/3rd/include/)
		message(INFO " using easy_profiler lib path " ${CMAKE_SOURCE_DIR}/3rd/)
		find_library(EASY_PROFILER_LIBRARY easy_profiler HINT ${CMAKE_SOURCE_DIR}/3rd/)
		add_definitions(-DINTEGRATE_EASY_PROFILER)
	else (NOT ${USE_PROFILER})
		message(INFO " skip easy_profiler")
endif (${USE_PROFILER})

add_executable(task_sheduller ${SOURCES})

target_link_libraries(task_sheduller ${EASY_PROFILER_LIBRARY} pthread)
