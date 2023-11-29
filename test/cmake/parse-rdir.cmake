# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Verify $Recycle.bin results match existing golden files
#

function(createRDirParseTests)
    set(ids DirEmpty DirVista DirWin10 DirUNC19)
    set(files dir-empty dir-sample1 dir-win10-01 dir-2019-uncpath)
    foreach(id file IN ZIP_LISTS ids files)
        generate_simple_comparison_test(${id} 0
            ${file} ${file}.txt "parse")
    endforeach()
endfunction()

createRDirParseTests()

#
# Similar to above, but only test single index file
#

generate_simple_comparison_test(DirOneIdx 0
    "dir-win10-01/$IKEGS1G" "dir-single-idx.txt" "parse")

#
# Similar to previous test, but copy index file elsewhere
# to test the isolated file behavior
#

add_test(NAME d_DirIsolatedIdx_PrepPre
    COMMAND ${CMAKE_COMMAND} -E copy ${sample_dir}/dir-win10-01/$IKEGS1G .)

add_test(NAME d_DirIsolatedIdx_Prep
    COMMAND rifiuti-vista $IKEGS1G -o d_DirIsolatedIdx.output)

add_test(NAME d_DirIsolatedIdx_CleanAlt
    COMMAND ${CMAKE_COMMAND} -E rm $IKEGS1G)

generate_simple_comparison_test(DirIsolatedIdx 0
    "" "dir-isolated-idx.txt" "parse")
