cmake_minimum_required(VERSION 4.2)

project(exceptionhandler4 LANGUAGES C CXX ASM_MASM)

##### Clangd ##################################################################

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(RELATIVE_PATH CLANGD_CDB_REL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
if(CLANGD_CDB_REL STREQUAL "")
  set(CLANGD_CDB_REL ".")
endif()

file(TO_CMAKE_PATH "${CLANGD_CDB_REL}" CLANGD_CDB_REL)

set(CLANGD_PROJECT_INCLUDE "${CMAKE_SOURCE_DIR}/include")
file(TO_CMAKE_PATH "${CLANGD_PROJECT_INCLUDE}" CLANGD_PROJECT_INCLUDE)

set(CLANGD_CONTENT
"CompileFlags:
  CompilationDatabase: ${CLANGD_CDB_REL}
  Remove: [-Werror]

---
If:
  PathMatch:
    - \"include/.*\\\\.(h|hpp)$\"
    - \"include/phnt/.*\\\\.(h|hpp)$\"
CompileFlags:
  Add:
    - \"-I${CLANGD_PROJECT_INCLUDE}\"
    - \"-isystem${CLANGD_PROJECT_INCLUDE}/phnt\"
")

file(WRITE "${CMAKE_SOURCE_DIR}/.clangd" "${CLANGD_CONTENT}")

if(CMAKE_GENERATOR MATCHES "Ninja" OR CMAKE_GENERATOR MATCHES "Makefile")
  add_custom_target(compdb ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Copying compile_commands.json to project root for clangd")
endif()

##### KNSoft's Hooking Library ################################################

add_library(knsoft_detours STATIC
    src/kndetours/Disassembler.c
    src/kndetours/InlineHook.c
    src/kndetours/Instruction.c
    src/kndetours/Memory.c
    src/kndetours/Thread.c
    src/kndetours/Trampoline.c
    src/kndetours/Transaction.c
    )

target_link_libraries(knsoft_detours PUBLIC ntdll)
target_compile_definitions(knsoft_detours PUBLIC _CRT_SECURE_NO_WARNINGS)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(knsoft_detours PRIVATE
        -Wno-pragma-pack
        -Wno-microsoft-static-assert
        -Wno-c23-extensions
        -Wno-microsoft-enum-forward-reference
        )
endif()

target_include_directories(knsoft_detours SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/phnt>
    )

##### Library #################################################################

add_library(eh4 STATIC
    src/eh4.c
    src/kiuserexceptiondispatcher.asm
)

target_link_libraries(eh4 PUBLIC ntdll knsoft_detours)

set_target_properties(eh4 PROPERTIES C_STANDARD 99)
set_target_properties(eh4 PROPERTIES C_EXTENSIONS OFF)

target_compile_definitions(eh4 PUBLIC _CRT_SECURE_NO_WARNINGS)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(eh4 PRIVATE
        -Wno-pragma-pack
        -Wno-microsoft-static-assert
        -Wno-c23-extensions
        -Wno-microsoft-enum-forward-reference
        )
endif()

target_include_directories(eh4 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/kndetours>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

# phnt.h is reasonable to have system header privileges due to a plethora
# of extensions warnings, and it's not our dependency. Static asserts will
# let us know if anything is actually wrong.
target_include_directories(eh4 SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/phnt>
    )

##### Watchdog ################################################################

add_executable(eh4_watchdog
    src/watchdog/main.c
    src/watchdog/process.c
    src/watchdog/cxxexception.c
    src/watchdog/util.c
    src/watchdog/unwind.c
    src/watchdog/xstate.c
    src/watchdog/registertable.c
    )

set(ENABLE_CJSON_TEST OFF)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(dep/cJSON)

add_dependencies(eh4_watchdog cjson knsoft_detours)
target_link_libraries(eh4_watchdog PUBLIC ntdll dbghelp cjson)

target_compile_definitions(cjson PUBLIC _CRT_SECURE_NO_WARNINGS)
target_compile_definitions(eh4_watchdog PUBLIC _CRT_SECURE_NO_WARNINGS)
target_include_directories(eh4_watchdog PUBLIC ${cJSON_SOURCE_DIR})

set_target_properties(eh4_watchdog PROPERTIES C_STANDARD 99)
set_target_properties(eh4_watchdog PROPERTIES C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(eh4_watchdog PRIVATE
        -Wno-pragma-pack
        -Wno-microsoft-static-assert
        -Wno-c23-extensions
        -Wno-microsoft-enum-forward-reference
        )
    target_compile_options(cjson PRIVATE
        -Wno-cast-qual
        -Wno-long-long
        -Wno-error=cast-qual
    )

endif()

target_include_directories(eh4_watchdog PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

target_include_directories(eh4_watchdog SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/phnt>
    )

##### Tests ###################################################################

option(EH4_BUILD_TESTS "Build tests" ON)

if(EH4_BUILD_TESTS)
    file(GLOB EH4_TEST_SOURCES tests/test_*.c tests/test_*.cpp)

    foreach(TEST_SOURCE ${EH4_TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        get_filename_component(TEST_DIR ${TEST_SOURCE} DIRECTORY)
        get_filename_component(TEST_EXT ${TEST_SOURCE} EXT)

        set(TEST_SOURCES ${TEST_SOURCE})

        set(TEST_ASM "${TEST_DIR}/${TEST_NAME}.asm")
        if(EXISTS ${TEST_ASM})
            message(STATUS "Found ASM for ${TEST_NAME}: ${TEST_ASM}")
            configure_file(${TEST_ASM} ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.asm COPYONLY)
            list(APPEND TEST_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}.asm)
        endif()

        add_executable(${TEST_NAME} ${TEST_SOURCES})
        target_compile_definitions(${TEST_NAME} PUBLIC _CRT_SECURE_NO_WARNINGS)

        if(MSVC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_link_options(${TEST_NAME} PRIVATE
                "LINKER:/CETCOMPAT"
            )
        endif()

        if(TEST_EXT STREQUAL ".cpp")
            set_target_properties(${TEST_NAME} PROPERTIES
                CXX_STANDARD 23
                CXX_EXTENSIONS OFF
            )
        else()
            set_target_properties(${TEST_NAME} PROPERTIES
                C_STANDARD 99
                C_EXTENSIONS OFF
            )
        endif()

        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            target_compile_options(${TEST_NAME} PRIVATE
                -Wno-pragma-pack
                -Wno-microsoft-static-assert
                -Wno-c23-extensions
                -Wno-microsoft-enum-forward-reference
            )
        endif()

        target_link_libraries(${TEST_NAME} PRIVATE eh4)
    endforeach()
endif()

##### Install #################################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS eh4 knsoft_detours
    EXPORT eh4Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS eh4_watchdog
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "phnt" EXCLUDE
)

install(EXPORT eh4Targets
    FILE eh4Targets.cmake
    NAMESPACE eh4::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eh4
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/eh4Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/eh4Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eh4
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/eh4ConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/eh4Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/eh4ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/eh4
)