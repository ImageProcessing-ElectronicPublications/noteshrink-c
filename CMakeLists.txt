cmake_minimum_required(VERSION 3.3)
project(noteshrink-c LANGUAGES C)

include_directories(include src/ext)

file(GLOB source_files include/*.h
                       src/*.c
                       CMakeLists.txt)
add_executable(noteshrink ${source_files})
target_link_libraries(noteshrink m)