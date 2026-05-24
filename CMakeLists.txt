cmake_minimum_required(VERSION 3.20)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR FILENAME PROJECT_NAME)
string(REPLACE " " "_" ProjectId ${PROJECT_NAME})
project(${PROJECT_NAME})

####
#include(setup-tools)

if(DEFINED PAYLOAD_DEPLOY)  
    set(PROG_DEPLOY ${PAYLOAD_DEPLOY})
    message(STATUS "PROG_DEPLOY: ${PROG_DEPLOY}")  
else()
    message(FATAL_ERROR "PAYLOAD_DEPLOY not set")
endif()

if(DEFINED ORBIS)
    set(PLATFORM orbis)
elseif(DEFINED PROSPERO)
    set(PLATFORM prospero)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()
####

add_compile_definitions(ELF_NAME="${PROJECT_NAME}.elf")
add_executable(${PROJECT_NAME}
        source/main.c
)

####
#include(optimization-settings)
add_definitions(
        -Wall
        -Werror
        -fplt
        -g
        -O0
        -fno-inline
        -fvisibility-nodllstorageclass=hidden
        -fms-extensions # for inline function information
        -Wno-microsoft-string-literal-from-predefined # for above
)
####

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        sqlite3
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}.elf)

add_custom_target(${PROJECT_NAME}_test
        USES_TERMINAL
        COMMAND ${PROG_DEPLOY} ${PROJECT_NAME}.elf
        DEPENDS ${PROJECT_NAME}
)
