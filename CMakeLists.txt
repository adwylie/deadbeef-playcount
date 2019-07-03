cmake_minimum_required(VERSION 3.14)
project(playcount LANGUAGES C VERSION 0.1 DESCRIPTION "DeaDBeeF Playcount Plugin")

find_file(API deadbeef/deadbeef.h)
if (NOT API)
    message(FATAL_ERROR "DeaDBeeF API MISSING")
else()
    message(STATUS "DeaDBeeF API present")
    get_filename_component(API_DIR ${API} DIRECTORY)
    include_directories(${API_DIR})
endif()

add_library(playcount SHARED playcount.c)
target_compile_options(playcount PRIVATE
        -std=c99 -shared -O3 -Wall -pedantic
        -DDDB_API_LEVEL=0 -DDDB_WARN_DEPRECATED=1
        -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR})

# Name our library 'playcount.so' instead of 'libplaycount.so'.
set_target_properties(playcount PROPERTIES PREFIX "")

# Install the library in the deadbeef lib folder.
set(ARCH_DEADBEEF_LIBDIR "/usr/lib/deadbeef")

if (EXISTS ${ARCH_DEADBEEF_LIBDIR})
    message(STATUS "DeaDBeef lib directory found")
    install(TARGETS playcount LIBRARY DESTINATION ${ARCH_DEADBEEF_LIBDIR})
else()
    message(FATAL_ERROR "DeaDBeeF lib directory UNKNOWN")
endif()