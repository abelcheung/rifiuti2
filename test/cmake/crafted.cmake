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

