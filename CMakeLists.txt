# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

cmake_minimum_required(VERSION 3.17 FATAL_ERROR) # cmake -E rm

project(rifiuti2
    VERSION 0.7.0
    HOMEPAGE_URL "https://github.com/abelcheung/rifiuti2/"
    DESCRIPTION "Windows recycle bin analysis tool"
    LANGUAGES C)

set(PROJECT_BUG_REPORT_URL "${PROJECT_HOMEPAGE_URL}issues")
set(PROJECT_TOOL_USAGE_URL "${PROJECT_HOMEPAGE_URL}wiki/Usage-and-Examples")
set(PROJECT_GH_PAGE "https://abelcheung.github.io/rifiuti2/")

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS_DEBUG "-O0 -g -Wall")
set(CMAKE_STATIC_LINKER_FLAGS "-static")

configure_file(src/config.h.in config.h)
configure_file(docs/rifiuti.1.in rifiuti.1)
configure_file(docs/readme.txt.in readme.txt)

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

# Install: Windows use simplistic folder,
# non-Windows follow FHS.
if(WIN32)
    set(CMAKE_INSTALL_BINDIR .)
    set(CMAKE_INSTALL_DOCDIR doc)
else()
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/rifiuti.1
        DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endif()
install(
    TARGETS
        rifiuti
        rifiuti-vista
    RUNTIME
)
install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${CMAKE_CURRENT_SOURCE_DIR}/docs/THANKS.txt
        ${CMAKE_CURRENT_BINARY_DIR}/readme.txt
    TYPE DOC
)

include(CTest)
add_subdirectory(test)

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_SOURCE_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
    set(CPACK_SOURCE_GENERATOR "TXZ")
endif()
set(CPACK_SOURCE_IGNORE_FILES
    .editorconfig
    .git/
    .gitattribute
    .gitignore
    .github/
    ${CMAKE_BINARY_DIR}/
    ${PROJECT_BINARY_DIR}/
)
include(CPack)
