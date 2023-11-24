#
# Verify INFO2 results match existing golden files
#

function(createInfo2TestSet prefix info2)

    set(fixture_name F_$<UPPER_CASE:${prefix}>)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/${prefix}.txt)
    set(ref ${CMAKE_CURRENT_SOURCE_DIR}/samples/${info2}.txt)
    set(args ${ARGN})

    add_test(NAME f_${prefix}_Prepare
        COMMAND rifiuti ${args} ${info2} -o ${out}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/samples
        COMMAND_EXPAND_LISTS)

    add_test(NAME f_${prefix}_Clean
        COMMAND ${CMAKE_COMMAND} -E rm ${out})

    add_test(NAME f_${prefix}
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${out} ${ref})

    set_tests_properties(f_${prefix}_Prepare PROPERTIES FIXTURES_SETUP    ${fixture_name})
    set_tests_properties(f_${prefix}_Clean   PROPERTIES FIXTURES_CLEANUP  ${fixture_name})
    set_tests_properties(f_${prefix}         PROPERTIES FIXTURES_REQUIRED ${fixture_name})

    set_tests_properties(f_${prefix}
        PROPERTIES
            LABELS "info2;read")

endfunction()


createInfo2TestSet(Info2Empty   INFO2-empty)
createInfo2TestSet(Info2WinNT   INFO-NT-en-1)
createInfo2TestSet(Info2Win98   INFO2-sample2 -l CP1252)
createInfo2TestSet(Info2WinME   INFO2-ME-en-1 -l CP1252)
createInfo2TestSet(Info2Win2K   INFO2-2k-cht-1)
createInfo2TestSet(Info2WinXP   INFO2-sample1)
createInfo2TestSet(Info2UNCA1   INFO2-me-en-uncpath -l ASCII)
createInfo2TestSet(Info2UNCU1   INFO2-03-tw-uncpath)

# In encoding.cmake now
# createInfo2TestSet(Info2Win95   INFO-95-ja-1 -l ${cp932})
# createInfo2TestSet(Info2UNCA2   INFO2-2k-tw-uncpath -l ${cp950})
