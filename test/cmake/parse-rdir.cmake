#
# Verify $Recycle.bin results match existing golden files
#

function(createRdirTestSet prefix dir)

    set(fixture_name D_$<UPPER_CASE:${prefix}>)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.txt)
    set(ref ${CMAKE_CURRENT_SOURCE_DIR}/samples/${dir}.txt)

    add_test(NAME d_${prefix}_Prepare
        COMMAND rifiuti-vista ${dir} -o ${out}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/samples)

    add_test(NAME d_${prefix}_Clean
        COMMAND ${CMAKE_COMMAND} -E rm ${out})

    add_test(NAME d_${prefix}
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${out} ${ref})

    set_tests_properties(d_${prefix}_Prepare PROPERTIES FIXTURES_SETUP    ${fixture_name})
    set_tests_properties(d_${prefix}_Clean   PROPERTIES FIXTURES_CLEANUP  ${fixture_name})
    set_tests_properties(d_${prefix}         PROPERTIES FIXTURES_REQUIRED ${fixture_name})

    set_tests_properties(d_${prefix}         PROPERTIES LABELS "recycledir;read")

endfunction()

createRdirTestSet(DirEmpty dir-empty)
createRdirTestSet(DirVista dir-sample1)
createRdirTestSet(DirWin10 dir-win10-01)
createRdirTestSet(DirUNC19 dir-2019-uncpath)

#
# Similar to above, but only test single index file
#

if(WIN32)
    add_test_using_shell(d_DirOneIdx_PrepareRef
        "Select-String -Path samples/dir-win10-01.txt -SimpleMatch 'IHO61YT' -Raw > d_DirOneIdx_r.txt")
else()
    add_test_using_shell(d_DirOneIdx_PrepareRef
        "grep 'IHO61YT' samples/dir-win10-01.txt > d_DirOneIdx_r.txt")
endif()

add_test(NAME d_DirOneIdx_PrepareOut
    COMMAND rifiuti-vista -n samples/dir-win10-01/$IHO61YT -o d_DirOneIdx_o.txt)

add_test(NAME d_DirOneIdx_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_DirOneIdx_r.txt d_DirOneIdx_o.txt)

add_test(NAME d_DirOneIdx
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol d_DirOneIdx_o.txt d_DirOneIdx_r.txt)

set_tests_properties(d_DirOneIdx_PrepareRef PROPERTIES FIXTURES_SETUP    D_DIRONEIDX)
set_tests_properties(d_DirOneIdx_PrepareOut PROPERTIES FIXTURES_SETUP    D_DIRONEIDX)
set_tests_properties(d_DirOneIdx_Clean      PROPERTIES FIXTURES_CLEANUP  D_DIRONEIDX)
set_tests_properties(d_DirOneIdx            PROPERTIES FIXTURES_REQUIRED D_DIRONEIDX)

set_tests_properties(d_DirOneIdx            PROPERTIES LABELS "recycledir;read")
