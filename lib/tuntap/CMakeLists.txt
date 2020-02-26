project(tuntap LANGUAGES C)
cmake_minimum_required(VERSION 3.0)

aux_source_directory(src LIB_SRC)
file(GLOB LIB_INC include/* )
add_library(tuntap STATIC ${LIB_SRC} ${LIB_INC})
target_compile_options(tuntap PUBLIC -O3 -Wall -std=gnu11)
target_include_directories(tuntap PUBLIC include)
target_link_libraries(tuntap PUBLIC pthread)
