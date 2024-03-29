# Copyright (C) 2023-2024, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

function(add_encoding_test name)
    set(args ${ARGN})
    list(PREPEND args -V)
    list(APPEND args -DTEST_GLIB_ICONV=$<TARGET_FILE:test_glib_iconv>)
    list(APPEND args -DRIFIUTI=$<TARGET_FILE:rifiuti>)
    list(APPEND args -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/_try_encoding.cmake)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND} ${args}
    )
endfunction()


function(add_encoding_test_with_cwd name cwd)
    set(args ${ARGN})
    list(PREPEND args -V)
    list(APPEND args -DTEST_GLIB_ICONV=$<TARGET_FILE:test_glib_iconv>)
    list(APPEND args -DRIFIUTI=$<TARGET_FILE:rifiuti>)
    list(APPEND args -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/_try_encoding.cmake)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND} ${args}
        WORKING_DIRECTORY ${cwd}
    )
endfunction()


#
# Encoding does not exist
#
add_encoding_test(f_EncNotExist -DCHOICES=xxx -DINFO2=dummy)
set_tests_properties(
    f_EncNotExist
    PROPERTIES
        LABELS "encoding;info2;xfail"
        PASS_REGULAR_EXPRESSION "'xxx' encoding is not supported"
)


#
# Legacy file (95-ME) requires encoding option
#
add_test(
    NAME f_EncNeeded
    COMMAND rifiuti INFO-95-ja-1
    WORKING_DIRECTORY ${sample_dir}
)
set_tests_properties(
    f_EncNeeded
    PROPERTIES
        LABELS "encoding;info2;xfail"
        PASS_REGULAR_EXPRESSION "produced on a legacy system without Unicode"
)


#
# ASCII incompatible encoding
#
if(APPLE)
    # iconv on Mac lacks many encodings
    add_encoding_test(f_IncompatEnc -DCHOICES=UTF-32 -DINFO2=dummy)
else()
    add_encoding_test(f_IncompatEnc -DCHOICES=IBM-037|IBM037|CP037 -DINFO2=dummy)
endif()
set_tests_properties(
    f_IncompatEnc
    PROPERTIES
        LABELS "encoding;info2;xfail"
        PASS_REGULAR_EXPRESSION "incompatible to any Windows code page"
)


#
# Legacy path encoding - correct (2 cases)
#

add_encoding_test_with_cwd(f_LegacyEncOK1_Prep
    ${sample_dir}
    -DINFO2=INFO2-sample1
    -DCHOICES=CP936|MS936|Windows-936|GBK|csGBK
    -DOUTFILE=${bindir}/f_LegacyEncOK1.output
)

generate_simple_comparison_test("LegacyEncOK1" 1
    "" "INFO2-sample1-alt.txt" "encoding")

add_encoding_test_with_cwd(f_LegacyEncOK2_Prep
    ${sample_dir}
    -DINFO2=INFO-95-ja-1
    -DCHOICES=CP932|Windows-932|IBM-943|SJIS|JIS_X0208|SHIFT_JIS|SHIFT-JIS
    -DOUTFILE=${bindir}/f_LegacyEncOK2.output
)

generate_simple_comparison_test("LegacyEncOK2" 1
    "" "INFO-95-ja-1.txt" "encoding")

#
# Legacy path encoding - wrong but still managed to get result
#
# Original file is in Windows ANSI (CP1252), but intentionally
# treat it as Shift-JIS, and got hex escapes
#

add_encoding_test_with_cwd(f_LegacyEncWrong_Prep
    ${sample_dir}
    -DINFO2=INFO2-sample2
    -DCHOICES=CP932|Windows-932|IBM-943|SJIS|JIS_X0208|SHIFT_JIS|SHIFT-JIS
    -DOUTFILE=${bindir}/f_LegacyEncWrong.output
)

set_tests_properties(f_LegacyEncWrong_Prep
    PROPERTIES
    PASS_REGULAR_EXPRESSION "could not be interpreted in .+ encoding")

generate_simple_comparison_test("LegacyEncWrong" 1
    "" "INFO2-sample2-wrong-enc.txt" "encoding|xfail")

#
# Legacy UNC entries
#

add_encoding_test_with_cwd(f_LegacyUNC_Prep
    ${sample_dir}
    -DINFO2=INFO2-2k-tw-uncpath
    -DCHOICES=CP950|Windows-950|BIG5
    -DOUTFILE=${bindir}/f_LegacyUNC.output
)

generate_simple_comparison_test("LegacyUNC" 1
    "" "INFO2-2k-tw-uncpath.txt" "encoding")

#
# JSON output
#

add_encoding_test_with_cwd(f_JsonInfo2Win95_Prep
    ${sample_dir}
    -DINFO2=INFO-95-ja-1
    -DCHOICES=CP932|Windows-932|IBM-943|SJIS|JIS_X0208|SHIFT_JIS|SHIFT-JIS
    -DOUTFILE=${bindir}/f_JsonInfo2Win95.output
    -DEXTRA_ARGS=-f|json
)

generate_simple_comparison_test("JsonInfo2Win95" 1
    "" "INFO-95-ja-1.json" "encoding|json")


add_encoding_test_with_cwd(f_JsonWrongEnc_Prep
    ${sample_dir}
    -DINFO2=INFO-95-ja-1
    -DCHOICES=CP1255|MS-HEBR|WINDOWS-1255|HEBREW|ISO-8859-8|ISO-IR-138|ISO8859-8|ISO_8859-8|ISO_8859-8:1988|CSISOLATINHEBREW
    -DOUTFILE=${bindir}/f_JsonWrongEnc.output
    -DEXTRA_ARGS=-f|json
)

set_tests_properties(f_JsonWrongEnc_Prep
    PROPERTIES
    PASS_REGULAR_EXPRESSION "could not be interpreted in .+ encoding")

generate_simple_comparison_test("JsonWrongEnc" 1
    "" "INFO-95-ja-1-in-cp1255.json" "encoding|xfail|json")

# It turns out different iconv implemention may have
# different behavior even for the same code page. Take this
# for example, GNU iconv marks 0x90 illegal for CP1255, but
# winiconv converts that to U+0090.
if(WIN32)
    set_tests_properties(f_JsonWrongEnc
    PROPERTIES WILL_FAIL true)
endif()


add_encoding_test_with_cwd(f_XmlWrongEnc_Prep
    ${sample_dir}
    -DINFO2=INFO-95-ja-1
    -DCHOICES=CP949|UHC|ISO-IR-149|KOREAN|KSC_5601|KS_C_5601-1987|KS_C_5601-1989|CSKSC56011987
    -DOUTFILE=${bindir}/f_XmlWrongEnc.output
    -DEXTRA_ARGS=-f|xml
)

set_tests_properties(f_XmlWrongEnc_Prep
    PROPERTIES
    PASS_REGULAR_EXPRESSION "could not be interpreted in .+ encoding")

generate_simple_comparison_test("XmlWrongEnc" 1
    "" "INFO-95-ja-1-in-cp949.xml" "encoding|xfail|xml")
