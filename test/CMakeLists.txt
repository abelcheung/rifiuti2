# Copyright (C) 2023-2024, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

# Shorthands
set(sample_dir ${CMAKE_CURRENT_SOURCE_DIR}/samples)
set(bindir ${CMAKE_CURRENT_BINARY_DIR})

set(
    CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

# Required by XML tests
find_program(XMLLINT xmllint)

# Util functions
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

function(startswith haystack needle)
    string(LENGTH ${needle} needle_len)
    string(SUBSTRING ${haystack} 0 ${needle_len} substr)

    if(${substr} STREQUAL ${needle})
        set(startswith_match 1 PARENT_SCOPE)
    else()
        set(startswith_match 0 PARENT_SCOPE)
    endif()
endfunction()

function(add_bintype_label testname)
    foreach(tname ${testname} ${ARGN})
        if(NOT TEST ${tname})
            message(WARNING "Test name ${tname} does not exist")
            continue()
        endif()
        startsWith(${tname} "f_")
        if(startswith_match)
            set(bintype "info2")
        endif()
        startsWith(${tname} "d_")
        if(startswith_match)
            set(bintype "recycledir")
        endif()
        if(NOT DEFINED bintype)
            message(WARNING "Unable to determine bin type from name ${tname}")
            continue()
        endif()
        get_property(tlabels TEST ${tname} PROPERTY LABELS)
        list(FIND tlabels ${bintype} pos)
        if (pos EQUAL -1)
            list(APPEND tlabels ${bintype})
            set_tests_properties(${tname} PROPERTIES LABELS "${tlabels}")
        endif()
    endforeach()
endfunction()

#
# Set fixture properties automatically for a fixed test name pattern.
# Fixture dependencies are set up automatically too.
# Using preparation step with test prefix "myid" as example:
#
# - "myid_Prep" (mandatory) test must exist beforehand.
# - "myid_PrepAlt" (optional) is treated as another independent setup step
# - "myid_PrepPre" (optional) is a prerequisite for "myid_Prep"
# - "myid_PrepPost" (optional) depends on "myid_Prep"
#
# Same applies to ${prefix}_Clean steps.
#
function(set_fixture_with_dep prefix)

    set(fixture $<UPPER_CASE:${prefix}>)
    set_tests_properties(${prefix} PROPERTIES FIXTURES_REQUIRED ${fixture})

    set(prepname ${prefix}_Prep)
    set_tests_properties(${prepname} PROPERTIES FIXTURES_SETUP ${fixture})

    set(prepalt ${prepname}Alt)
    if(TEST ${prepalt})
        set_tests_properties(${prepalt} PROPERTIES FIXTURES_SETUP ${fixture})
    endif()

    set(preppre ${prepname}Pre)
    if(TEST ${preppre})
        set_tests_properties(${preppre} PROPERTIES FIXTURES_SETUP ${fixture})
        set_tests_properties(${prepname} PROPERTIES DEPENDS ${preppre})
    endif()

    set(preppost ${prepname}Post)
    if(TEST ${preppost})
        set_tests_properties(${preppost} PROPERTIES FIXTURES_SETUP ${fixture})
        set_tests_properties(${preppost} PROPERTIES DEPENDS ${prepname})
    endif()

    set(cleanname ${prefix}_Clean)
    set_tests_properties(${cleanname} PROPERTIES FIXTURES_CLEANUP  ${fixture})

    set(cleanalt ${cleanname}Alt)
    if(TEST ${cleanalt})
        set_tests_properties(${cleanalt} PROPERTIES FIXTURES_CLEANUP ${fixture})
    endif()

    set(cleanpre ${cleanname}Pre)
    if(TEST ${cleanpre})
        set_tests_properties(${cleanpre} PROPERTIES FIXTURES_CLEANUP ${fixture})
        set_tests_properties(${cleanname} PROPERTIES DEPENDS ${cleanpre})
    endif()

    set(cleanpost ${cleanname}Post)
    if(TEST ${cleanpost})
        set_tests_properties(${cleanpost} PROPERTIES FIXTURES_CLEANUP ${fixture})
        set_tests_properties(${cleanpost} PROPERTIES DEPENDS ${cleanname})
    endif()

endfunction()

#
# Simplistic comparison involving 3 steps:
# 1. Geneate output from test recycle bin file (use -o output, no redirection used)
# 2. Compare output with reference test result
# 3. Delete output file
#
# This function create tests with add_test(), fixtures and label properties. Other properties should be added manually. Output file name is detemined automatically from test prefix. Extra arguments are appended to program command line arguments.
#
# Parameters:
# id (string): unique test ID fragment, will be prepended with "f_" or "d_" to form full test prefix name, which is determined by 'is_info2' param below.
# is_info2 (bool): See 'id' param.
# input (path): Recycle bin file/dir name or full path, to be read by rifiuti2.
# - If it is a relative path, add_test() calls would set WORKING_DIRECTORY to sample folder in source dir (i.e. ${sample_dir} above).
# - If it is empty or falsy, the whole preparation step is skipped. User is responsible to create their own step BEFORE calling this function (not after, otherwise fixture automation won't work).
# Note that the path is reflected in output file content, thus can potentially make tests fail if not careful.
# ref (path): Reference test result file name or path. If this is relative path, it is taken as under ${sample_dir}.
# label (str): A '|'-separated list of labels, to prevent automatic expansion. The labels are only applied to main test, not other fixture steps.
#
function(generate_simple_comparison_test
    id is_info2 input ref labels)
    if(is_info2)
        set(prefix f_${id})
        set(progname rifiuti)
    else()
        set(prefix d_${id})
        set(progname rifiuti-vista)
    endif()

    set(out ${bindir}/${prefix}.output)
    set(remnant ${out})

    if(NOT IS_ABSOLUTE ${ref})
        set(ref ${sample_dir}/${ref})
    else()
        startswith(${ref} ${bindir})
        if(startswith_match)
            list(APPEND remnant ${ref})
        endif()
    endif()

    if(input)
        if(IS_ABSOLUTE ${input})
            add_test(NAME ${prefix}_Prep
                COMMAND ${progname} -o ${out} ${ARGN} ${input}
                COMMAND_EXPAND_LISTS)
        else()
            add_test(NAME ${prefix}_Prep
                COMMAND ${progname} -o ${out} ${ARGN} ${input}
                WORKING_DIRECTORY ${sample_dir}
                COMMAND_EXPAND_LISTS)
        endif()
    endif()

    add_test(NAME ${prefix}
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${out} ${ref})

    add_test(NAME ${prefix}_Clean
        COMMAND ${CMAKE_COMMAND} -E rm ${remnant}
        COMMAND_EXPAND_LISTS)

    set_fixture_with_dep(${prefix})

    string(REPLACE "|" ";" labels "${labels}")
    if(is_info2)
        list(APPEND labels "info2")
    else()
        list(APPEND labels "recycledir")
    endif()
    set_tests_properties(${prefix} PROPERTIES LABELS "${labels}")
endfunction()


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
include(json)
include(parse-info2)
include(parse-rdir)
include(read-write)
include(xml)
