cmake_minimum_required(VERSION 3.21)
project(flos LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

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

if(NOT EMSCRIPTEN)
    add_subdirectory(glfw)
    add_subdirectory(webgpu)
endif()

set(SOURCES
    main.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cglm
        marrow
        printccy
        ripple
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -pedantic
    -Wall
    -Wextra
    -Werror
    -Wno-missing-field-initializers
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-misleading-indentation
    -Wno-bitfield-constant-conversion
    -Wno-language-extension-token
    -Wno-gnu-zero-variadic-macro-arguments
)

if(EMSCRIPTEN)
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
else()
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            webgpu
            glfw
    )

    target_copy_webgpu_binaries(${PROJECT_NAME})

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/res"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/res"
        COMMENT "Copying resources"
    )
endif()
