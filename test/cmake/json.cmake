# Copyright (C) 2023-2024, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Verify JSON output works as intended
#

function(createJsonOutputTests)

set(ids
    "JsonInfo2Empty" "JsonInfo2WinXP" "JsonInfo2Win98"
    "JsonRdirVista" "JsonRdirWin10" "JsonRdirUNC19"
)

set(files
    "INFO2-empty" "INFO2-sample1" "INFO2-sample2"
    "dir-sample1" "dir-win10-01" "dir-2019-uncpath"
)

set(encs
    "" "" "CP1252" "" ""
)

foreach(id file enc IN ZIP_LISTS ids files encs)
    if (IS_DIRECTORY ${sample_dir}/${file})
        set(is_info2 0)
    else()
        set(is_info2 1)
    endif()
    set(args -f json)
    if(enc)
        list(APPEND args -l ${enc})
    endif()
    generate_simple_comparison_test(${id} ${is_info2}
        ${file} ${file}.json "parse|json" ${args})
endforeach()

endfunction()

createJsonOutputTests()
