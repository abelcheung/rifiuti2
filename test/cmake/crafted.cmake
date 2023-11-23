#
# Mix different file versions into a single dir
#

add_test(
    NAME d_CraftedMixVer
    COMMAND rifiuti-vista samples/dir-mixed
)

set_tests_properties(
    d_CraftedMixVer
    PROPERTIES
        LABELS "recycledir;crafted"
        PASS_REGULAR_EXPRESSION "come from multiple versions")

#
# Create dir with bad permission
#

add_test(
    NAME BadPermDirCopy
    COMMAND ${CMAKE_COMMAND} -E copy_directory samples/dir-win10-01 dir-BadPerm
)

if(WIN32)
    add_test(
        NAME BadPermDirPrepPerm
        COMMAND icacls.exe dir-BadPerm /q /inheritance:r /grant:r "Users:(OI)(CI)(S,REA)"
    )
    add_test(
        NAME BadPermDirRestorePerm
        COMMAND icacls.exe dir-BadPerm /q /reset
    )
else()
    add_test(
        NAME BadPermDirPrepPerm
        COMMAND chmod u= dir-BadPerm
    )
    add_test(
        NAME BadPermDirRestorePerm
        COMMAND chmod u=rwx dir-BadPerm
    )
endif()

add_test(
    NAME BadPermDirClean
    COMMAND ${CMAKE_COMMAND} -E rm -r dir-BadPerm
)

add_test(
    NAME d_BadPermDir
    COMMAND rifiuti-vista dir-BadPerm
)

set_tests_properties(
    d_BadPermDir
    PROPERTIES
        LABELS "recycledir;crafted"
        PASS_REGULAR_EXPRESSION "Permission denied;disallowed under Windows ACL"
)

set_tests_properties(BadPermDirCopy        PROPERTIES FIXTURES_SETUP    D_BADPERMDIR)
set_tests_properties(BadPermDirPrepPerm    PROPERTIES FIXTURES_SETUP    D_BADPERMDIR)
set_tests_properties(d_BadPermDir          PROPERTIES FIXTURES_REQUIRED D_BADPERMDIR)
set_tests_properties(BadPermDirRestorePerm PROPERTIES FIXTURES_CLEANUP  D_BADPERMDIR)
set_tests_properties(BadPermDirClean       PROPERTIES FIXTURES_CLEANUP  D_BADPERMDIR)

set_tests_properties(BadPermDirPrepPerm    PROPERTIES DEPENDS BadPermDirCopy)
set_tests_properties(BadPermDirClean       PROPERTIES DEPENDS BadPermDirRestorePerm)


#
# Simulate bad UTF-16 path entries
#

add_test(NAME d_BadUniEnc_Prep
    COMMAND rifiuti-vista dir-bad-uni -o ${CMAKE_CURRENT_BINARY_DIR}/d_BadUniEnc.txt
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/samples
)

add_test(
    NAME d_BadUniEnc
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol d_BadUniEnc.txt samples/dir-bad-uni.txt
)

add_test(
    NAME d_BadUniEnc_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_BadUniEnc.txt
)

set_tests_properties(d_BadUniEnc_Prep  PROPERTIES FIXTURES_SETUP    D_BADUNIENC)
set_tests_properties(d_BadUniEnc       PROPERTIES FIXTURES_REQUIRED D_BADUNIENC)
set_tests_properties(d_BadUniEnc_Clean PROPERTIES FIXTURES_CLEANUP  D_BADUNIENC)

set_tests_properties(d_BadUniEnc_Prep
    PROPERTIES
    PASS_REGULAR_EXPRESSION "displayed in escaped unicode sequences")
set_tests_properties(d_BadUniEnc
    PROPERTIES
        LABELS "recycledir;encoding;crafted")
