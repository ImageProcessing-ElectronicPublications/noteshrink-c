cmake_minimum_required(VERSION 2.8)
project(noteshrink-c LANGUAGES C)

include_directories(include include src/ext)

set(source_files
    src/dependencies.c
    src/noteshrink.c
    src/main.c
)

if(MSVC)
list(APPEND source_files src/ext/win-c/source/getopt.c)
include_directories(src/ext/win-c/include)
endif()

add_executable(noteshrink ${source_files})

if(NOT MSVC)
target_link_libraries(noteshrink m)
endif()
