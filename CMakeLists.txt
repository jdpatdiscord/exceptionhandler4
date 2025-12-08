cmake_minimum_required(VERSION 4.2)

project(exceptionhandler4 LANGUAGES C CXX)

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

##### Library #################################################################

add_library(eh4
    src/eh4.c
)

set_target_properties(eh4 PROPERTIES C_STANDARD 99)
set_target_properties(eh4 PROPERTIES C_EXTENSIONS OFF)

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
    src/eh4_agent.c
    src/eh4_agent_processing.c
    )

set_target_properties(eh4_watchdog PROPERTIES C_STANDARD 99)
set_target_properties(eh4_watchdog PROPERTIES C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    target_compile_options(eh4_watchdog PRIVATE
        -Wno-pragma-pack
        -Wno-microsoft-static-assert
        -Wno-c23-extensions
        -Wno-microsoft-enum-forward-reference
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
    enable_testing()

    file(GLOB EH4_TEST_SOURCES tests/test_*.c tests/test_*.cpp)

    foreach(TEST_SOURCE ${EH4_TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        get_filename_component(TEST_EXT ${TEST_SOURCE} EXT)
        
        add_executable(${TEST_NAME} ${TEST_SOURCE})

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

        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()


##### Install #################################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS eh4
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