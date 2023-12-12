# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Non-existant input file arg
#

add_test(NAME d_InputNotExist COMMAND rifiuti-vista dUmMy)
add_test(NAME f_InputNotExist COMMAND rifiuti       dUmMy)

set_tests_properties(d_InputNotExist f_InputNotExist
    PROPERTIES
        LABELS "xfail"
        PASS_REGULAR_EXPRESSION "does not exist")
add_bintype_label(d_InputNotExist f_InputNotExist)


#
# Special file
#
# g_file_test() is unuseful in Windows; NUL, CON etc are
# treated as regular files, therefore can't be determined
# ahead as unusable. They have to be actually opened.
#
if(WIN32)
add_test(NAME d_InputSpecialFile COMMAND rifiuti-vista nul)
add_test(NAME f_InputSpecialFile COMMAND rifiuti       nul)
set_tests_properties(d_InputSpecialFile f_InputSpecialFile
    PROPERTIES
        LABELS "xfail"
        PASS_REGULAR_EXPRESSION "File is prematurely truncated, or not .+ index")
else()
add_test(NAME d_InputSpecialFile COMMAND rifiuti-vista /dev/null)
add_test(NAME f_InputSpecialFile COMMAND rifiuti       /dev/null)
set_tests_properties(d_InputSpecialFile f_InputSpecialFile
    PROPERTIES
        LABELS "xfail"
        PASS_REGULAR_EXPRESSION "not a normal file")
endif()
add_bintype_label(d_InputSpecialFile f_InputSpecialFile)


#
# File output == Console output?
#

function(FileStdoutCompareTest testid input)

    if(IS_DIRECTORY ${sample_dir}/${input})
        set(is_info2 0)
        set(prog rifiuti-vista)
        set(prefix d_${testid})
    else()
        set(is_info2 1)
        set(prog rifiuti)
        set(prefix f_${testid})
    endif()

    set(con_output ${bindir}/${prefix}_c.txt)

    add_test_using_shell(${prefix}_PrepAlt
        "$<TARGET_FILE:${prog}> ${input} > ${con_output}"
        WORKING_DIRECTORY ${sample_dir})

    generate_simple_comparison_test(${testid} ${is_info2}
        "${input}" "${con_output}" "write")

endfunction()

FileStdoutCompareTest("FileConDiffA" "dir-sample1")
FileStdoutCompareTest("FileConDiffU" "dir-win10-01")
FileStdoutCompareTest("FileConDiffF" "INFO2-03-tw-uncpath")

#
# Unicode filename / dir name should work
#

generate_simple_comparison_test("UnicodePathName" 1
    "./ごみ箱/INFO2-empty" "japanese-path-file.txt" "encoding")
generate_simple_comparison_test("UnicodePathName" 0
    "./ごみ箱/dir-empty" "japanese-path-dir.txt" "encoding")
