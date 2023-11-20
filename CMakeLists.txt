cmake_minimum_required(VERSION 3.14 FATAL_ERROR) # install(PROGRAM TYPE)

project(rifiuti2
    VERSION 0.7.0
    HOMEPAGE_URL https://github.com/abelcheung/rifiuti2/
    LANGUAGES C)

if(NOT WIN32)
    include(GNUInstallDirs)
endif()

add_subdirectory(test)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
set(glib_min_ver 2.40.0)  # g_win32_get_command_line()
pkg_check_modules(GLIB REQUIRED "glib-2.0 >= ${glib_min_ver}")

configure_file(config.h.in config.h)
include_directories(PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
include_directories(PRIVATE ${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
link_libraries(${GLIB_LIBRARIES})
set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb -Wall")
 
foreach(bin rifiuti rifiuti-vista)
    add_executable(
        ${bin}
        src/${bin}.c
        src/${bin}.h
    )
    target_sources(
        ${bin}
        PRIVATE
            src/utils.c
            src/utils.h
    )
    if (WIN32)
        target_sources(
            ${bin}
            PRIVATE
                src/utils-win.c
                src/utils-win.h
        )
        target_link_libraries(${bin} authz)
    endif()
    target_compile_options(
        ${bin}
        PRIVATE
            ${GLIB_CFLAGS_OTHER}
    )
endforeach()

install(
    TARGETS
        rifiuti
        rifiuti-vista
    RUNTIME
)
if(NOT WIN32)
    install(
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/rifiuti.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()
install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/NEWS.md
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/THANKS.txt
    TYPE DOC
)
