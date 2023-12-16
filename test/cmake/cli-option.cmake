# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

function(addBareOptTest name)
    add_test(NAME d_InvokeOpt${name} COMMAND rifiuti-vista ${ARGV1})
    add_test(NAME f_InvokeOpt${name} COMMAND rifiuti       ${ARGV1})
    set_tests_properties(d_InvokeOpt${name} f_InvokeOpt${name}
        PROPERTIES LABELS "arg")
    add_bintype_label(d_InvokeOpt${name} f_InvokeOpt${name})
    if(NOT DEFINED ARGV1 AND WIN32)
        set_tests_properties(d_InvokeOpt${name} f_InvokeOpt${name}
            PROPERTIES DISABLED True)
    endif()
endfunction()

addBareOptTest(None                   )
addBareOptTest(ShortHelp1 -h          )
addBareOptTest(ShortHelp2 -?          )
addBareOptTest(ShortVer   -v          )
addBareOptTest(LongHelp   --help-all  )
addBareOptTest(LongVer    --version   )


function(addWithFileOptTest name)
    add_test(NAME d_WithFileOpt${name}
        COMMAND rifiuti-vista ${ARGN} ${sample_dir}/dir-sample1)
    add_test(NAME f_WithFileOpt${name}
        COMMAND rifiuti       ${ARGN} ${sample_dir}/INFO2-sample1)
    set_tests_properties(d_WithFileOpt${name} f_WithFileOpt${name}
        PROPERTIES LABELS "arg")
    add_bintype_label(d_WithFileOpt${name} f_WithFileOpt${name})
endfunction()

addWithFileOptTest(LongHead   --no-heading  )
addWithFileOptTest(LongSep    --delimiter=: )
addWithFileOptTest(LongTime   --localtime   )
addWithFileOptTest(LongXml    --format xml  )
addWithFileOptTest(ShortHead  -n            )
addWithFileOptTest(ShortSep   -t :          )
addWithFileOptTest(ShortTime  -z            )
addWithFileOptTest(ShortXml   -f xml        )


function(addBadBareOptTest name)
    add_test(NAME d_BadBareOpt${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_BadBareOpt${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(d_BadBareOpt${name} f_BadBareOpt${name}
        PROPERTIES
            LABELS "arg;xfail"
            PASS_REGULAR_EXPRESSION "Unknown option")
    add_bintype_label(d_BadBareOpt${name} f_BadBareOpt${name})
endfunction()

addBadBareOptTest(Short    -/)
addBadBareOptTest(Long     --invalid)


function(addDupOptTest name)
    add_test(NAME d_DupOpt${name} COMMAND
        rifiuti-vista ${ARGN} ${sample_dir}/dir-sample1)
    add_test(NAME f_DupOpt${name} COMMAND
        rifiuti       ${ARGN} ${sample_dir}/INFO2-sample1)
    set_tests_properties(d_DupOpt${name} f_DupOpt${name}
        PROPERTIES
            LABELS "arg;xfail"
            PASS_REGULAR_EXPRESSION "Multiple .+ disallowed")
    add_bintype_label(d_DupOpt${name} f_DupOpt${name})
endfunction()

addDupOptTest(ShortSep  -t ":" -t ","                 )
addDupOptTest(LongSep   --delimiter=: --delimiter=/   )
addDupOptTest(MixSep    --delimiter=: -t /            )
addDupOptTest(ShortOut  -o file1 -o file2             )
addDupOptTest(LongOut   --output=file1 --output=file2 )
addDupOptTest(MixOut    --output=file1 -o file2       )


add_test(NAME f_DupOptShortEnc COMMAND
    rifiuti -l ASCII -l CP1252 ${sample_dir}/INFO2-sample2)
add_test(NAME f_DupOptLongEnc COMMAND
    rifiuti --legacy-filename=ASCII --legacy-filename=CP1252 ${sample_dir}/INFO2-sample2)
add_test(NAME f_DupOptMixEnc COMMAND
    rifiuti -l ASCII --legacy-filename=CP1252 ${sample_dir}/INFO2-sample2)
set_tests_properties(
    f_DupOptShortEnc
    f_DupOptLongEnc
    f_DupOptMixEnc
    PROPERTIES
        LABELS "info2;arg;xfail"
        PASS_REGULAR_EXPRESSION "Multiple .+ disallowed")


add_test(NAME d_NullArgOptTestOut
    COMMAND rifiuti-vista -o "" ${sample_dir}/dir-sample1)
add_test(NAME f_NullArgOptTestOut
    COMMAND rifiuti       -o "" ${sample_dir}/INFO2-sample1)
add_test(NAME f_NullArgOptTestEnc
    COMMAND rifiuti       -l "" ${sample_dir}/INFO2-sample1)
set_tests_properties(d_NullArgOptTestOut f_NullArgOptTestOut f_NullArgOptTestEnc
    PROPERTIES
        LABELS "arg;xfail"
        PASS_REGULAR_EXPRESSION "Empty .+ disallowed")
add_bintype_label(d_NullArgOptTestOut f_NullArgOptTestOut f_NullArgOptTestEnc)


function(addBadComboOptTest id)
    add_test(NAME d_BadComboOptTest${id} COMMAND
        rifiuti-vista ${ARGN} ${sample_dir}/dir-sample1)
    add_test(NAME f_BadComboOptTest${id} COMMAND
        rifiuti       ${ARGN} ${sample_dir}/INFO2-sample1)
    set_tests_properties(d_BadComboOptTest${id} f_BadComboOptTest${id}
        PROPERTIES
            LABELS "arg;xfail"
            PASS_REGULAR_EXPRESSION "Output was already set in .+ format, but later argument attempts to change to .+ format")
    add_bintype_label(d_BadComboOptTest${id} f_BadComboOptTest${id})
endfunction()

# implicit text options
addBadComboOptTest(1 -f xml -t:)
addBadComboOptTest(2 -n -f xml)
# explicit option conflict
addBadComboOptTest(3 -f tsv -f json)
addBadComboOptTest(4 -f xml -f text)


function(addMultiInputTest name)
    add_test(NAME d_MultiInputTest${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_MultiInputTest${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(d_MultiInputTest${name} f_MultiInputTest${name}
        PROPERTIES
            LABELS "arg;xfail"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
    add_bintype_label(d_MultiInputTest${name} f_MultiInputTest${name})
endfunction()

addMultiInputTest(1 a a)
addMultiInputTest(2 foo bar baz)


function(addMissingInputTest name)
    add_test(NAME d_MissingInputTest${name} COMMAND rifiuti-vista ${ARGN})
    add_test(NAME f_MissingInputTest${name} COMMAND rifiuti       ${ARGN})
    set_tests_properties(d_MissingInputTest${name} f_MissingInputTest${name}
        PROPERTIES
            LABELS "arg;xfail"
            PASS_REGULAR_EXPRESSION "Must specify exactly one")
    add_bintype_label(d_MissingInputTest${name} f_MissingInputTest${name})
endfunction()

addMissingInputTest(1 -f xml)
addMissingInputTest(2 -t :)
addMissingInputTest(3 -z -o file1 -n)


function(SepCompareTest testid input sep)
    if(IS_DIRECTORY ${sample_dir}/${input})
        set(is_info2 0)
        set(prefix d_${testid})
    else()
        set(is_info2 1)
        set(prefix f_${testid})
    endif()

    set(ref ${bindir}/${prefix}_ref.txt)

    if(WIN32)
        string(REPLACE \\ ` pssep ${sep})
        add_test_using_shell(${prefix}_PrepAlt
            "(Get-Content ${sample_dir}/${input}.txt).Replace(\"`t\", \"${pssep}\") | Set-Content ${ref}")
    else()
        add_test_using_shell(${prefix}_PrepAlt
            "awk '{gsub(\"\\t\",\"${sep}\");print;}' ${sample_dir}/${input}.txt > ${ref}")
    endif()

    generate_simple_comparison_test(${testid} ${is_info2}
        "${input}" "${ref}" "arg" -t "${sep}")

endfunction()

function(addSepCompareTests)
    # "\\\\" converts to "\;" in cmake, can't do test on backslashes
    set(seps "|" "\\n\\t" "%s")
    list(LENGTH seps len)
    math(EXPR len "${len} - 1")
    foreach(i RANGE ${len})
        list(GET seps ${i} sep)
        SepCompareTest(SepCompare${i} "INFO2-sample1" "${sep}")
        SepCompareTest(SepCompare${i} "dir-sample1" "${sep}")
    endforeach()
endfunction()

addSepCompareTests()

# Live option
# TODO think about possibility to configure recycle bin
# specifically for GitHub Windows Server runner

add_test(NAME d_LiveProbeOpt
    COMMAND rifiuti-vista --live)
set_tests_properties(d_LiveProbeOpt
    PROPERTIES
        LABELS "recycledir;arg"
        SKIP_RETURN_CODE 1
        PASS_REGULAR_EXPRESSION "\\(current system\\)")

