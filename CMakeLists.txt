cmake_minimum_required(VERSION 2.8)
project(noteshrink-c LANGUAGES C)

include_directories(include include src/ext)

set(source_files
    src/dependencies.c
    src/noteshrink.c
    src/main.c
)
add_executable(noteshrink ${source_files})
target_link_libraries(noteshrink m)
