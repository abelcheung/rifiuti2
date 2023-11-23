function(addBareOptTest name)
    add_test(NAME d_InvokeOpt${name} COMMAND rifiuti-vista ${ARGV1})
    add_test(NAME f_InvokeOpt${name} COMMAND rifiuti       ${ARGV1})
    set_tests_properties(d_InvokeOpt${name} PROPERTIES LABELS "recycledir;arg")
    set_tests_properties(f_InvokeOpt${name} PROPERTIES LABELS      "info2;arg")
    if(NOT DEFINED ARGV1 AND WIN32)
        set_tests_properties(d_InvokeOpt${name} PROPERTIES DISABLED True)
        set_tests_properties(f_InvokeOpt${name} PROPERTIES DISABLED True)
    endif()
endfunction()

addBareOptTest(None                   )
addBareOptTest(ShortHelp1 -h          )
addBareOptTest(ShortHelp2 -?          )
addBareOptTest(ShortVer   -v          )
addBareOptTest(LongHelp   --help-all  )
addBareOptTest(LongVer    --version   )
# if(WIN32)
#     addBareOptTest(Live   --live      )
# endif()

function(addWithFileOptTest name)
    add_test(NAME d_WithFileOpt${name} COMMAND rifiuti-vista ${ARGN} samples/dir-sample1)
    add_test(NAME f_WithFileOpt${name} COMMAND rifiuti       ${ARGN} samples/INFO2-sample1)
    set_tests_properties(d_WithFileOpt${name} PROPERTIES LABELS "recycledir;arg")
    set_tests_properties(f_WithFileOpt${name} PROPERTIES LABELS      "info2;arg")
endfunction()

addWithFileOptTest(LongHead   --no-heading  )
addWithFileOptTest(LongSep    --delimiter=: )
addWithFileOptTest(LongTime   --localtime   )
addWithFileOptTest(LongXml    --xml         )
addWithFileOptTest(ShortHead  -n            )
addWithFileOptTest(ShortSep   -t :          )
addWithFileOptTest(ShortTime  -z            )
addWithFileOptTest(ShortXml   -x            )


function(addBadBareOptTest name)
    add_test(NAME d_BadBareOpt${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_BadBareOpt${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(d_BadBareOpt${name}
        PROPERTIES
            LABELS "recycledir;arg"
            PASS_REGULAR_EXPRESSION "Unknown option")
    set_tests_properties(f_BadBareOpt${name}
        PROPERTIES
            LABELS      "info2;arg"
            PASS_REGULAR_EXPRESSION "Unknown option")
endfunction()

addBadBareOptTest(Short    -/)
addBadBareOptTest(Long     --invalid)


function(addDupOptTest name)
    add_test(NAME d_DupOpt${name} COMMAND
        rifiuti-vista ${ARGN} samples/dir-sample1)
    add_test(NAME f_DupOpt${name} COMMAND
        rifiuti       ${ARGN} samples/INFO2-sample1)
    set_tests_properties(d_DupOpt${name}
        PROPERTIES
            LABELS "recycledir;arg"
            PASS_REGULAR_EXPRESSION "Multiple .+ disallowed")
    set_tests_properties(f_DupOpt${name}
        PROPERTIES
            LABELS      "info2;arg"
            PASS_REGULAR_EXPRESSION "Multiple .+ disallowed")
endfunction()

addDupOptTest(ShortSep    -t ":" -t ","               )
addDupOptTest(LongSep     --delimiter=: --delimiter=/ )
addDupOptTest(MixSep      --delimiter=: -t /          )
addDupOptTest(ShortOut    -o file1 -o file2              )
addDupOptTest(LongOut     --output=file1 --output=file2  )
addDupOptTest(MixOut      --output=file1 -o file2        )


add_test(NAME f_DupOptShortEnc COMMAND
    rifiuti -l ASCII -l CP1252 samples/INFO2-sample2)
add_test(NAME f_DupOptLongEnc COMMAND
    rifiuti --legacy-filename=ASCII --legacy-filename=CP1252 samples/INFO2-sample2)
add_test(NAME f_DupOptMixEnc COMMAND
    rifiuti -l ASCII --legacy-filename=CP1252 samples/INFO2-sample2)
set_tests_properties(
    f_DupOptShortEnc
    f_DupOptLongEnc
    f_DupOptMixEnc
    PROPERTIES
        LABELS "info2;arg"
        PASS_REGULAR_EXPRESSION "Multiple .+ disallowed")


add_test(NAME d_NullArgOptTestOut COMMAND rifiuti-vista -o "" samples/dir-sample1)
add_test(NAME f_NullArgOptTestOut COMMAND rifiuti       -o "" samples/INFO2-sample1)
add_test(NAME f_NullArgOptTestEnc COMMAND rifiuti       -l "" samples/INFO2-sample1)
set_tests_properties(
    d_NullArgOptTestOut
    PROPERTIES
        LABELS "recycledir;arg"
        PASS_REGULAR_EXPRESSION "Empty .+ disallowed")
set_tests_properties(
    f_NullArgOptTestOut
    f_NullArgOptTestEnc
    PROPERTIES
        LABELS "info2;arg"
        PASS_REGULAR_EXPRESSION "Empty .+ disallowed")


function(addBadComboOptTest name)
    add_test(NAME d_BadComboOptTest${name} COMMAND
        rifiuti-vista ${ARGN} samples/dir-sample1)
    add_test(NAME f_BadComboOptTest${name} COMMAND
        rifiuti       ${ARGN} samples/INFO2-sample1)
    set_tests_properties(
        d_BadComboOptTest${name}
        PROPERTIES
            LABELS "recycledir;arg"
            PASS_REGULAR_EXPRESSION "can not be used in XML mode")
    set_tests_properties(
        f_BadComboOptTest${name}
        PROPERTIES
            LABELS      "info2;arg"
            PASS_REGULAR_EXPRESSION "can not be used in XML mode")
endfunction()

addBadComboOptTest(1 -x -t:)
addBadComboOptTest(2 -n -x)


function(addMultiInputTest name)
    add_test(NAME d_MultiInputTest${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_MultiInputTest${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(
        d_MultiInputTest${name}
        PROPERTIES
            LABELS "recycledir;arg"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
    set_tests_properties(
        f_MultiInputTest${name}
        PROPERTIES
            LABELS      "info2;arg"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
endfunction()

addMultiInputTest(1 a a)
addMultiInputTest(2 foo bar baz)


function(addMissingInputTest name)
    add_test(NAME d_MissingInputTest${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_MissingInputTest${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(
        d_MissingInputTest${name}
        PROPERTIES
            LABELS "recycledir;arg"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
    set_tests_properties(
        f_MissingInputTest${name}
        PROPERTIES
            LABELS      "info2;arg"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
endfunction()

addMissingInputTest(1 -x)
addMissingInputTest(2 -t :)
addMissingInputTest(3 -z -o file1 -n)


function(SepCompareTest testid is_info2 sep)
    set_label(is_info2 "arg")
    set_test_vars(${testid} is_info2 1 1)
    set(out ${CMAKE_CURRENT_BINARY_DIR}/${prefix}_o.txt)
    set(ref ${CMAKE_CURRENT_BINARY_DIR}/${prefix}_r.txt)
    if(is_info2)
        set(prog rifiuti)
        set(input INFO2-sample1)
    else()
        set(prog rifiuti-vista)
        set(input dir-sample1)
    endif()
    add_test(
        NAME ${prefix}_Prep1
        COMMAND ${prog} -t "${sep}" ${input} -o ${out}
        WORKING_DIRECTORY ${sample_dir}
    )
    if(WIN32)
        string(REPLACE \\ ` sep ${sep})
        add_test_using_shell(${prefix}_Prep2
            "(Get-Content ${input}.txt).Replace(\"`t\", \"${sep}\") | Set-Content ${ref}"
            WORKING_DIRECTORY ${sample_dir})
    else()
        add_test_using_shell(${prefix}_Prep2
            "awk '{gsub(\"\\t\",\"${sep}\");print;}' ${input}.txt > ${ref}"
            WORKING_DIRECTORY ${sample_dir})
    endif()
    add_test(
        NAME ${prefix}
        COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${out} ${ref}
    )
    add_test(
        NAME ${prefix}_Clean
        COMMAND ${CMAKE_COMMAND} -E rm ${out} ${ref}
    )

    set_tests_properties(${prefix}_Prep1 PROPERTIES FIXTURES_SETUP    ${fixture})
    set_tests_properties(${prefix}_Prep2 PROPERTIES FIXTURES_SETUP    ${fixture})
    set_tests_properties(${prefix}       PROPERTIES FIXTURES_REQUIRED ${fixture})
    set_tests_properties(${prefix}_Clean PROPERTIES FIXTURES_CLEANUP  ${fixture})

    set_tests_properties(${prefix}       PROPERTIES LABELS "${label}")
endfunction()

# "\\\\" converts to "\;" in cmake, can't do test on backslashes
set(nums 1 2 3 4)
set(seps "|" "xx" "\\n\\t" "%s")
foreach(item IN ZIP_LISTS nums seps)
    SepCompareTest(SepCompare${item_0} 0 "${item_1}")
    SepCompareTest(SepCompare${item_0} 1 "${item_1}")
endforeach()
