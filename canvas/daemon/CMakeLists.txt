cmake_minimum_required(VERSION 3.22)

project(ojbs_daemon C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ojbsd
    offsets.c
    vecmath.c
    protocol.c
    memread.c
    ipc.c
    chain.c
    selftest.c
    main.c
)

target_compile_options(ojbsd PRIVATE
    -Wall
    -Wextra
    -O2
    -fno-exceptions
)

target_link_libraries(ojbsd m)

# Niente PIE non serve: Android dal 2017 pretende eseguibili PIE.
set_target_properties(ojbsd PROPERTIES POSITION_INDEPENDENT_CODE ON)
