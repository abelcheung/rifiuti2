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
        LABELS "encoding;info2"
        PASS_REGULAR_EXPRESSION "'xxx' encoding is not supported"
)


#
# Legacy file (95-ME) requires encoding option
#
add_test(
    NAME f_EncNeeded
    COMMAND rifiuti INFO-95-ja-1
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/samples
)
set_tests_properties(
    f_EncNeeded
    PROPERTIES
        LABELS "encoding;info2"
        PASS_REGULAR_EXPRESSION "produced on a legacy system without Unicode"
)


#
# ASCII incompatible encoding
#
add_encoding_test(f_BadEncEBCDIC -DCHOICES=IBM-037|IBM037|CP037 -DINFO2=dummy)
set_tests_properties(
    f_BadEncEBCDIC
    PROPERTIES
        LABELS "encoding;info2"
        PASS_REGULAR_EXPRESSION "possibly be a code page or compatible encoding"
)


#
# Legacy path encoding - correct (case 1)
#

add_encoding_test_with_cwd(f_LegacyEncOK1_Prep
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    -DINFO2=INFO2-sample1
    -DCHOICES=CP936|MS936|Windows-936|GBK|csGBK
    -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/f_LegacyEncOK1.txt
)

add_test(
    NAME f_LegacyEncOK1
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol f_LegacyEncOK1.txt samples/INFO2-sample1-alt.txt
)

add_test(
    NAME f_LegacyEncOK1_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_LegacyEncOK1.txt
)

set_tests_properties(f_LegacyEncOK1_Prep  PROPERTIES FIXTURES_SETUP    F_LEGACYENCOK1)
set_tests_properties(f_LegacyEncOK1       PROPERTIES FIXTURES_REQUIRED F_LEGACYENCOK1)
set_tests_properties(f_LegacyEncOK1_Clean PROPERTIES FIXTURES_CLEANUP  F_LEGACYENCOK1)

set_tests_properties(f_LegacyEncOK1       PROPERTIES LABELS "info2;encoding")


#
# Legacy path encoding - correct (case 2)
#

add_encoding_test_with_cwd(f_LegacyEncOK2_Prep
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    -DINFO2=INFO-95-ja-1
    -DCHOICES=CP932|Windows-932|IBM-943|SJIS|JIS_X0208|SHIFT_JIS|SHIFT-JIS
    -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/f_LegacyEncOK2.txt
)

add_test(
    NAME f_LegacyEncOK2
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol f_LegacyEncOK2.txt samples/INFO-95-ja-1.txt
)

add_test(
    NAME f_LegacyEncOK2_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_LegacyEncOK2.txt
)

set_tests_properties(f_LegacyEncOK2_Prep  PROPERTIES FIXTURES_SETUP    F_LEGACYENCOK2)
set_tests_properties(f_LegacyEncOK2       PROPERTIES FIXTURES_REQUIRED F_LEGACYENCOK2)
set_tests_properties(f_LegacyEncOK2_Clean PROPERTIES FIXTURES_CLEANUP  F_LEGACYENCOK2)

set_tests_properties(f_LegacyEncOK2       PROPERTIES LABELS "info2;encoding")


#
# Legacy path encoding - wrong but still managed to get result
#
# Original file is in Windows ANSI (CP1252), but intentionally
# treat it as Shift-JIS, and got hex escapes
#

add_encoding_test_with_cwd(f_LegacyEncWrong_Prep
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    -DINFO2=INFO2-sample2
    -DCHOICES=CP932|Windows-932|IBM-943|SJIS|JIS_X0208|SHIFT_JIS|SHIFT-JIS
    -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/f_LegacyEncWrong.txt
)

add_test(
    NAME f_LegacyEncWrong
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol f_LegacyEncWrong.txt samples/INFO2-sample2-wrong-enc.txt
)

add_test(
    NAME f_LegacyEncWrong_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_LegacyEncWrong.txt
)

set_tests_properties(f_LegacyEncWrong_Prep  PROPERTIES FIXTURES_SETUP    F_LEGACYENCWRONG)
set_tests_properties(f_LegacyEncWrong       PROPERTIES FIXTURES_REQUIRED F_LEGACYENCWRONG)
set_tests_properties(f_LegacyEncWrong_Clean PROPERTIES FIXTURES_CLEANUP  F_LEGACYENCWRONG)

set_tests_properties(f_LegacyEncWrong_Prep
    PROPERTIES
    PASS_REGULAR_EXPRESSION "does not use specified codepage")
set_tests_properties(f_LegacyEncWrong
    PROPERTIES
        LABELS "info2;encoding")


#
# Legacy UNC entries
#

add_encoding_test_with_cwd(f_LegacyUNC_Prep
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    -DINFO2=INFO2-2k-tw-uncpath
    -DCHOICES=CP950|Windows-950|BIG5
    -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/f_LegacyUNC.txt
)

add_test(
    NAME f_LegacyUNC
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol f_LegacyUNC.txt samples/INFO2-2k-tw-uncpath.txt
)

add_test(
    NAME f_LegacyUNC_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_LegacyUNC.txt
)

set_tests_properties(f_LegacyUNC_Prep  PROPERTIES FIXTURES_SETUP    F_LEGACYUNC)
set_tests_properties(f_LegacyUNC       PROPERTIES FIXTURES_REQUIRED F_LEGACYUNC)
set_tests_properties(f_LegacyUNC_Clean PROPERTIES FIXTURES_CLEANUP  F_LEGACYUNC)

set_tests_properties(f_LegacyUNC
    PROPERTIES
        LABELS "info2;encoding")
