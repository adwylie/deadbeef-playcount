cmake_minimum_required(VERSION 3.14)
project(playcount LANGUAGES C VERSION 1.0 DESCRIPTION "DeaDBeeF Playcount Plugin")

find_file(API deadbeef/deadbeef.h)
if (NOT API)
    message(FATAL_ERROR "DeaDBeeF API: MISSING")
else()
    message(STATUS "DeaDBeeF API: present")
    get_filename_component(API_DIR ${API} DIRECTORY)
    include_directories(${API_DIR})
endif()

add_library(playcount SHARED playcount.c id3v2.c)
set_property(TARGET playcount PROPERTY C_STANDARD 99)

set(CMAKE_C_FLAGS_DEBUG "-g -Og -Wall -pedantic -DDEBUG")
target_compile_options(playcount PRIVATE
        -DDDB_API_LEVEL=0 -DDDB_WARN_DEPRECATED=1
        -DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        -DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR})

# Name our library 'playcount.so' instead of 'libplaycount.so'.
set_target_properties(playcount PROPERTIES PREFIX "")

# Install the library in the deadbeef lib folder.
set(DEADBEEF_LIBDIR "~/.local/lib/deadbeef/")
message(STATUS "Install path: \"${DEADBEEF_LIBDIR}\"")
install(TARGETS playcount LIBRARY DESTINATION ${DEADBEEF_LIBDIR})
