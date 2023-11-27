# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

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

add_test(NAME d_DirOneIdx_Prep
    COMMAND rifiuti-vista samples/dir-win10-01/$IKEGS1G -o ${CMAKE_CURRENT_BINARY_DIR}/d_DirOneIdx.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME d_DirOneIdx
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol d_DirOneIdx.txt ${CMAKE_CURRENT_SOURCE_DIR}/samples/dir-single-idx.txt)

add_test(NAME d_DirOneIdx_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_DirOneIdx.txt)

set_tests_properties(d_DirOneIdx_Prep  PROPERTIES FIXTURES_SETUP    D_DIRONEIDX)
set_tests_properties(d_DirOneIdx_Clean PROPERTIES FIXTURES_CLEANUP  D_DIRONEIDX)
set_tests_properties(d_DirOneIdx       PROPERTIES FIXTURES_REQUIRED D_DIRONEIDX)

set_tests_properties(d_DirOneIdx       PROPERTIES LABELS "recycledir;read")


#
# Same as previous test, but copy index file elsewhere
# to test the isolated file behavior
#

add_test(NAME d_DirIsolatedIdx_Prep1
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/samples/dir-win10-01/$IKEGS1G .)

add_test(NAME d_DirIsolatedIdx_Prep2
    COMMAND rifiuti-vista $IKEGS1G -o d_DirIsolatedIdx.txt)

add_test(NAME d_DirIsolatedIdx
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol d_DirIsolatedIdx.txt ${CMAKE_CURRENT_SOURCE_DIR}/samples/dir-isolated-idx.txt)

add_test(NAME d_DirIsolatedIdx_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_DirIsolatedIdx.txt $IKEGS1G)

set_tests_properties(d_DirIsolatedIdx_Prep1 PROPERTIES FIXTURES_SETUP    D_DIRISOLATEDIDX)
set_tests_properties(d_DirIsolatedIdx_Prep2 PROPERTIES FIXTURES_SETUP    D_DIRISOLATEDIDX)
set_tests_properties(d_DirIsolatedIdx_Clean PROPERTIES FIXTURES_CLEANUP  D_DIRISOLATEDIDX)
set_tests_properties(d_DirIsolatedIdx       PROPERTIES FIXTURES_REQUIRED D_DIRISOLATEDIDX)

set_tests_properties(d_DirIsolatedIdx_Prep2 PROPERTIES DEPENDS d_DirIsolatedIdx_Prep1)
set_tests_properties(d_DirIsolatedIdx       PROPERTIES LABELS "recycledir;read")
