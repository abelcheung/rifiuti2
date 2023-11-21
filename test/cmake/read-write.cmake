#
# Non-existant input file arg
#

add_test(NAME d_InputNotExist COMMAND rifiuti-vista dUmMy)
add_test(NAME f_InputNotExist COMMAND rifiuti       dUmMy)

set_tests_properties(d_InputNotExist
    PROPERTIES
        LABELS "recycledir;read"
        PASS_REGULAR_EXPRESSION "does not exist")
set_tests_properties(f_InputNotExist
    PROPERTIES
        LABELS      "info2;read"
        PASS_REGULAR_EXPRESSION "does not exist")

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
else()
add_test(NAME d_InputSpecialFile COMMAND rifiuti-vista /dev/null)
add_test(NAME f_InputSpecialFile COMMAND rifiuti       /dev/null)
endif()

set_tests_properties(d_InputSpecialFile
    PROPERTIES
        LABELS "recycledir;read"
        PASS_REGULAR_EXPRESSION "fails validation")
set_tests_properties(f_InputSpecialFile
    PROPERTIES
        LABELS      "info2;read"
        PASS_REGULAR_EXPRESSION "fails validation")

# TODO Consider symbolic links support

#
# File output == Console output?
#

function(createComparisonTestSet prefix sample_dir)

    set(fixture_name D_$<UPPER_CASE:${prefix}>)
    set(out1 ${prefix}_f.txt)
    set(out2 ${prefix}_c.txt)

    add_test(NAME d_${prefix}_PrepareFile
        COMMAND rifiuti-vista -o ${out1} ${sample_dir})
    add_test_using_shell(
        d_${prefix}_PrepareCon
        "$<TARGET_FILE:rifiuti-vista> ${sample_dir} > ${out2}")

    set_tests_properties(
        d_${prefix}_PrepareFile
        d_${prefix}_PrepareCon
        PROPERTIES
            FIXTURES_SETUP ${fixture_name})

    add_test(NAME d_${prefix}_CleanFile
        COMMAND ${CMAKE_COMMAND} -E rm ${out1})
    add_test(NAME d_${prefix}_CleanCon
        COMMAND ${CMAKE_COMMAND} -E rm ${out2})

    set_tests_properties(
        d_${prefix}_CleanFile
        d_${prefix}_CleanCon
        PROPERTIES
            FIXTURES_CLEANUP ${fixture_name})

    add_test(NAME d_${prefix}
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${out1} ${out2})

    set_tests_properties(d_${prefix}
        PROPERTIES
            LABELS "recycledir;read;write"
            FIXTURES_REQUIRED ${fixture_name})

endfunction()

createComparisonTestSet(FileConDiffA samples/dir-sample1)
createComparisonTestSet(FileConDiffU samples/dir-win10-01)

