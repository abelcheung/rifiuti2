# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Verify INFO2 results match existing golden files
#

function(createINFO2ParseTests)
# Keep variables scoped
set(ids
    "Info2Empty" "Info2WinNT" "Info2Win98" "Info2WinME"
    "Info2Win2K" "Info2WinXP" "Info2UNCA1" "Info2UNCU1")

set(files
    "INFO2-empty" "INFO-NT-en-1"
    "INFO2-sample2" "INFO2-ME-en-1"
    "INFO2-2k-cht-1" "INFO2-sample1"
    "INFO2-me-en-uncpath" "INFO2-03-tw-uncpath")

set(encs
    "" "" "CP1252" "CP1252" "" "" "ASCII" "")

foreach(id file enc IN ZIP_LISTS ids files encs)
    if(enc)
        generate_simple_comparison_test(${id} 1
            ${file} ${file}.txt "parse" -l ${enc})
    else()
        generate_simple_comparison_test(${id} 1
            ${file} ${file}.txt "parse")
    endif()
endforeach()

endfunction()

createINFO2ParseTests()

# In encoding.cmake now
# (Info2Win95   INFO-95-ja-1 -l ${cp932})
# (Info2UNCA2   INFO2-2k-tw-uncpath -l ${cp950})
