cmake_minimum_required(VERSION 3.0)

project(rifiuti2
    VERSION 0.7.0
    HOMEPAGE_URL https://github.com/abelcheung/rifiuti2/
    LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
if(WIN32)  # g_win32_get_command_line()
    pkg_check_modules(GLIB REQUIRED "glib-2.0 >= 2.40.0")
else()
    pkg_check_modules(GLIB REQUIRED "glib-2.0 >= 2.28.0")
endif()

configure_file(config.h.in config.h)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBRARY_DIRS})
link_libraries(${GLIB_LIBRARIES})

add_executable(
    rifiuti
    src/rifiuti.c
    src/rifiuti.h
    src/utils.c
    src/utils.h
)
add_executable(
    rifiuti-vista
    src/rifiuti-vista.c
    src/rifiuti-vista.h
    src/utils.c
    src/utils.h
)

add_compile_options(
    PRIVATE
        ${GLIB_CFLAGS_OTHER}
)

if (WIN32)
    target_sources(
        rifiuti
        PRIVATE
            src/utils-win.c
            src/utils-win.h
    )
    target_sources(
        rifiuti-vista
        PRIVATE
            src/utils-win.c
            src/utils-win.h
    )
    target_link_libraries(rifiuti authz)
    target_link_libraries(rifiuti-vista authz)
endif()
