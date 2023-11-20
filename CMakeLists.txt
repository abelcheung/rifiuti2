cmake_minimum_required(VERSION 3.17 FATAL_ERROR) # cmake -E rm

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

set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb -Wall")
set(CMAKE_STATIC_LINKER_FLAGS "-static")
configure_file(config.h.in config.h)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED "glib-2.0 >= 2.40.0")

# Do static build in Windows, which require finding
# extra libraries
if (WIN32)
pkg_check_modules(ICONV REQUIRED "iconv")
list(APPEND GLIB_STATIC_CFLAGS_OTHER -DGLIB_STATIC_COMPILATION)
endif()

foreach(bin rifiuti rifiuti-vista)
    add_executable(
        ${bin}
        src/${bin}.c
        src/${bin}.h
    )
    target_include_directories(
        ${bin} BEFORE
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}
    )
    target_sources(
        ${bin}
        PRIVATE
            src/utils.c
            src/utils.h
    )
    if(WIN32)
        target_sources(${bin} PRIVATE src/utils-win.c src/utils-win.h)
        target_include_directories(${bin} PRIVATE
            ${GLIB_STATIC_INCLUDE_DIRS} ${ICONV_STATIC_INCLUDE_DIRS})
        target_compile_options    (${bin} PRIVATE
            ${GLIB_STATIC_CFLAGS_OTHER} ${ICONV_STATIC_CFLAGS_OTHER})
        target_link_libraries     (${bin} PRIVATE authz
            ${GLIB_STATIC_LIBRARIES} ${ICONV_STATIC_LIBRARIES})
        target_link_directories   (${bin} PRIVATE
            ${GLIB_STATIC_LIBRARY_DIRS} ${ICONV_STATIC_LIBRARY_DIRS})
        target_link_options       (${bin} BEFORE PRIVATE ${CMAKE_STATIC_LINKER_FLAGS})
    else()
        target_include_directories(${bin} PRIVATE ${GLIB_INCLUDE_DIRS})
        target_compile_options    (${bin} PRIVATE ${GLIB_CFLAGS_OTHER})
        target_link_libraries     (${bin} PRIVATE ${GLIB_LIBRARIES})
        target_link_directories   (${bin} PRIVATE ${GLIB_LIBRARY_DIRS})
    endif()
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
