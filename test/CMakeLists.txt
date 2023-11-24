set(
    CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

file(
    CREATE_LINK
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    ${CMAKE_CURRENT_BINARY_DIR}/samples
    SYMBOLIC
)

function(add_test_using_shell name command)
    if(WIN32)
        add_test(
            NAME ${name}
            COMMAND pwsh -NonInteractive -NoProfile -Command "${command}"
            ${ARGN})
    else()
        add_test(
            NAME ${name}
            COMMAND sh -c "${command}"
            ${ARGN})
    endif()
endfunction()

macro(set_label is_info2)
    set(label ${ARGN})
    if(is_info2)
        list(APPEND label "info2")
    else()
        list(APPEND label "recycledir")
    endif()
endmacro()

macro(set_test_vars id is_info2 has_output has_fixture)
    if(is_info2)
        set(prefix f_${id})
    else()
        set(prefix d_${id})
    endif()
    set(sample_dir ${CMAKE_CURRENT_SOURCE_DIR}/samples)
    if(${has_output})
        set(out ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.txt)
    endif()
    if(${has_fixture})
        set(fixture $<UPPER_CASE:${prefix}>)
    endif()
endmacro()

#
# For some systems, glib may or may not be using system iconv
# (e.g. Solaris and FreeBSD), therefore simply finding out
# supported encoding from iconv program is not enough.
#
# In general, there is no cross platform way of specifying
# encoding names. To rub salt into wounds, even glib itself
# appears to append different encoding aliases on different
# OSes... (such as using win_iconv on Windows)
# Have to resort to checking runtime behavior instead.
#
add_executable(test_glib_iconv test_glib_iconv.c)
target_include_directories(test_glib_iconv PRIVATE ${GLIB_INCLUDE_DIRS})
target_compile_options    (test_glib_iconv PRIVATE ${GLIB_CFLAGS_OTHER})
target_link_libraries     (test_glib_iconv PRIVATE ${GLIB_LIBRARIES})
target_link_directories   (test_glib_iconv PRIVATE ${GLIB_LIBRARY_DIRS})

#
# The real tests
#
include(cli-option)
include(crafted)
include(encoding)
include(parse-info2)
include(parse-rdir)
include(read-write)
include(xml)
