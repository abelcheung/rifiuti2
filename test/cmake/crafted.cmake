# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Mix different file versions into a single dir
#

add_test(
    NAME d_CraftedMixVer
    COMMAND rifiuti-vista ${sample_dir}/dir-mixed
)

set_tests_properties(
    d_CraftedMixVer
    PROPERTIES
        LABELS "recycledir;crafted;xfail"
        PASS_REGULAR_EXPRESSION "Index files from multiple Windows versions")

#
# Create dir with bad permission
#

add_test(
    NAME d_BadPermDir_Prep
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${sample_dir}/dir-win10-01 dir-BadPerm
)

if(WIN32)
    add_test(NAME d_BadPermDir_PrepPost
        COMMAND icacls.exe dir-BadPerm /q /inheritance:r /grant:r "Users:(OI)(CI)(S,REA)")
    add_test(NAME d_BadPermDir_CleanPre
        COMMAND icacls.exe dir-BadPerm /q /reset)
else()
    add_test(NAME d_BadPermDir_PrepPost
        COMMAND chmod u= dir-BadPerm)
    add_test(NAME d_BadPermDir_CleanPre
        COMMAND chmod u=rwx dir-BadPerm)
endif()

add_test(NAME d_BadPermDir_Clean
    COMMAND ${CMAKE_COMMAND} -E rm -r dir-BadPerm)

add_test(NAME d_BadPermDir
    COMMAND rifiuti-vista dir-BadPerm)

set_tests_properties(
    d_BadPermDir
    PROPERTIES
        LABELS "recycledir;crafted;xfail"
        PASS_REGULAR_EXPRESSION "Permission denied;disallowed under Windows ACL")

set_fixture_with_dep("d_BadPermDir")


#
# Simulate bad UTF-16 path entries
#

generate_simple_comparison_test("BadUniEnc" 0
    "dir-bad-uni" "dir-bad-uni.txt" "encoding|crafted|xfail")

set_tests_properties(d_BadUniEnc_Prep
    PROPERTIES
        PASS_REGULAR_EXPRESSION "Path contains broken unicode character\\(s\\)")
