# Copyright (C) 2023-2024, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# XML well-formed, DTD validation, normalization checks
#

function(createXmlTestSet id input)  # $ARGN as extra rifiuti args

    set(dtd ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd)

    if(NOT IS_ABSOLUTE ${input})
        set(inputchk ${sample_dir}/${input})
    else()
        set(inputchk ${input})
    endif()
    if(IS_DIRECTORY ${inputchk})
        set(is_info2 0)
        set(prog rifiuti-vista)
        set(prefix "d_Xml${id}")
    else()
        set(is_info2 1)
        set(prog rifiuti)
        set(prefix "f_Xml${id}")
    endif()

    set(wellform_pfx "${prefix}WellForm")
    set(dtdvalid_pfx "${prefix}DTDValidate")
    set(xmlequal_pfx "${prefix}Equal")

    set(wellform_out "${bindir}/${wellform_pfx}.output")
    set(wellform_fxt $<UPPER_CASE:${wellform_pfx}>)

    set(xmlequal_out "${bindir}/${xmlequal_pfx}.output")
    set(xmlequal_ref "${bindir}/${xmlequal_pfx}.refout")
    set(xmlequal_fxt $<UPPER_CASE:${xmlequal_pfx}>)

    # XML/DTD validation part
    add_test(NAME ${wellform_pfx}_Prep
        COMMAND ${prog} -o ${wellform_out} ${ARGN} -x ${input}
        WORKING_DIRECTORY ${sample_dir})

    add_test(NAME ${wellform_pfx}
        COMMAND ${XMLLINT} --noout ${wellform_out})

    add_test(NAME ${dtdvalid_pfx}
        COMMAND ${XMLLINT} --noout ${wellform_out} --dtdvalid ${dtd})

    add_test(NAME ${wellform_pfx}_Clean
        COMMAND ${CMAKE_COMMAND} -E rm ${wellform_out})

    # XmlWellForm fixtures are used in multiple fixtures,
    # properties set further down below, not here
    set_tests_properties(${wellform_pfx}
        PROPERTIES FIXTURES_REQUIRED ${wellform_fxt})
    set_tests_properties(${dtdvalid_pfx}
        PROPERTIES FIXTURES_REQUIRED ${wellform_fxt})
    set_tests_properties(${wellform_pfx} ${dtdvalid_pfx}
        PROPERTIES LABELS "xml")
    add_bintype_label(${wellform_pfx} ${dtdvalid_pfx})

    # XML normalization and comparison
    # xmllint has a long known history of broken --output option,
    # have to use redirection instead.
    # https://unix.stackexchange.com/q/492116

    add_test_using_shell(${xmlequal_pfx}_PrepAlt
        "${XMLLINT} --c14n ${wellform_out} > ${xmlequal_out}")
    set_tests_properties(${xmlequal_pfx}_PrepAlt
        PROPERTIES DEPENDS ${wellform_pfx}_Prep)

    add_test_using_shell(${xmlequal_pfx}_Prep
        "${XMLLINT} --c14n ${input}.xml > ${xmlequal_ref}"
        WORKING_DIRECTORY ${sample_dir})

    generate_simple_comparison_test("Xml${id}Equal" ${is_info2}
        "" "${xmlequal_ref}" "xml")

    set_fixture_with_dep("${xmlequal_pfx}")

    set_tests_properties(${wellform_pfx}_Prep
        PROPERTIES FIXTURES_SETUP "${wellform_fxt};${xmlequal_fxt}")
    set_tests_properties(${wellform_pfx}_Clean
        PROPERTIES FIXTURES_CLEANUP "${wellform_fxt};${xmlequal_fxt}")

    if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
        set_tests_properties(
            ${dtdvalid_pfx}
            ${xmlequal_pfx}
            ${xmlequal_pfx}_Clean
            ${xmlequal_pfx}_PrepAlt
            ${xmlequal_pfx}_Prep
            ${wellform_pfx}
            ${wellform_pfx}_Clean
            ${wellform_pfx}_Prep
            PROPERTIES DISABLED true)
    endif()
endfunction()

createXmlTestSet(1 INFO2-sample1)
createXmlTestSet(2 dir-sample1)
createXmlTestSet(3 INFO-95-ja-1 -l CP932)


# Make sure no version is printed if $Recycle.bin
# is empty because no index can be found; but empty
# INFO2 still contains version info
generate_simple_comparison_test (NoVerIfDirEmpty 0
    dir-empty dir-empty.xml "xml" -x)

generate_simple_comparison_test (HasVerIfInfo2Empty 1
    INFO2-empty INFO2-empty.xml "xml" -x)
