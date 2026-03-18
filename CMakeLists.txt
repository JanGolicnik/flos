cmake_minimum_required(VERSION 3.21)
project(flos LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(FetchContent)

set(THIRD_PARTY_DEPS cglm marrow printccy ripple)

set(FETCH_CGLM_REPO      "https://github.com/recp/cglm.git")
set(FETCH_CGLM_TAG       "v0.9.5")
set(FETCH_MARROW_REPO    "https://github.com/JanGolicnik/marrow.git")
set(FETCH_MARROW_TAG     "master")
set(FETCH_PRINTCCY_REPO  "https://github.com/JanGolicnik/printccy.git")
set(FETCH_PRINTCCY_TAG   "master")
set(FETCH_RIPPLE_REPO    "https://github.com/JanGolicnik/ripple.git")
set(FETCH_RIPPLE_TAG     "master")

foreach(dep IN LISTS THIRD_PARTY_DEPS)
    string(TOUPPER "${dep}" dep_upper)
    set("${dep_upper}_SOURCE_DIR" CACHE PATH "local path to ${dep}")
endforeach()

foreach(dep IN LISTS THIRD_PARTY_DEPS)
    string(TOUPPER "${dep}" dep_upper)

    if(DEFINED ${dep_upper}_SOURCE_DIR AND NOT "${${dep_upper}_SOURCE_DIR}" STREQUAL "")
        message(STATUS "Using local ${dep}: ${${dep_upper}_SOURCE_DIR}")
        FetchContent_Declare(
            ${dep}
            SOURCE_DIR "${${dep_upper}_SOURCE_DIR}"
        )
    else()
        message(STATUS "Fetching ${dep} from ${FETCH_${dep_upper}_REPO} @ ${FETCH_${dep_upper}_TAG}")
        FetchContent_Declare(
            ${dep}
            GIT_REPOSITORY "${FETCH_${dep_upper}_REPO}"
            GIT_TAG        "${FETCH_${dep_upper}_TAG}"
            GIT_SHALLOW    TRUE
        )
    endif()
endforeach()

FetchContent_MakeAvailable(${THIRD_PARTY_DEPS})

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
