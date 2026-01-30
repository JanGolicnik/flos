cmake_minimum_required(VERSION 3.10)
project(flos)

set(C_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
  cglm
  GIT_REPOSITORY https://github.com/recp/cglm.git
  GIT_TAG        v0.9.5
)
FetchContent_Declare(
    marrow
    GIT_REPOSITORY https://github.com/JanGolicnik/marrow.git
)
FetchContent_Declare(
    printccy
    GIT_REPOSITORY https://github.com/JanGolicnik/printccy.git
)
FetchContent_Declare(
    ripple
    GIT_REPOSITORY https://github.com/JanGolicnik/ripple.git
)

FetchContent_MakeAvailable(cglm marrow printccy ripple)

if (NOT EMSCRIPTEN)
    add_subdirectory(glfw)
    add_subdirectory(webgpu)
endif()

set(SOURCES main.c)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pedantic -Wall -Wextra -Werror -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-function -Wno-bitfield-constant-conversion -Wno-misleading-indentation")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if (EMSCRIPTEN)
    target_compile_options(${PROJECT_NAME} PRIVATE
        --use-port=emdawnwebgpu
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-language-extension-token
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        --use-port=emdawnwebgpu
        -sASYNCIFY
        --embed-file "${CMAKE_CURRENT_SOURCE_DIR}/res@/res"
        -sSTACK_SIZE=100048576
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")

    target_link_libraries(${PROJECT_NAME}
        cglm
        marrow
        printccy
        ripple
    )
else()
    target_link_libraries(${PROJECT_NAME}
        cglm
        marrow
        printccy
        ripple
        webgpu
        glfw
    )

    target_copy_webgpu_binaries(${PROJECT_NAME})
endif()
