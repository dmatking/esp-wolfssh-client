cmake_minimum_required(VERSION 3.16)

# Tell the build system where to find the component under development.
# Change this path if you clone the component to a different location.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(basic_connect)
