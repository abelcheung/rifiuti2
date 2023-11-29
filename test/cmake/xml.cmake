# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

set(myDTD ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd)

# XML well-formed and DTD validation for INFO2

add_test(NAME f_XmlWellForm_Prep
    COMMAND rifiuti -o ${bindir}/f_XmlWellForm.xml -x INFO2-sample1
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME f_XmlWellForm
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml)

add_test(NAME f_XmlDTDValidate
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME f_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_XmlWellForm.xml)

# f_XmlWellForm_Prep/Clean are used in multiple fixtures,
# properties set further down below
set_tests_properties(f_XmlWellForm    PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)
set_tests_properties(f_XmlDTDValidate PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)

set_tests_properties(
    f_XmlWellForm f_XmlDTDValidate
    PROPERTIES LABELS "info2;xml")


# XML equality test for INFO2

# xmllint has a long known history that its --output option is broken.
# Have to use redirection instead.
# https://unix.stackexchange.com/q/492116

add_test_using_shell(f_XmlEqual_PrepAlt
    "${XMLLINT} --c14n f_XmlWellForm.xml > ${bindir}/f_XmlEqual.output")
set_tests_properties(f_XmlEqual_PrepAlt
    PROPERTIES DEPENDS f_XmlWellForm_Prep)

add_test_using_shell(f_XmlEqual_Prep
    "${XMLLINT} --c14n INFO2-sample1.xml > ${bindir}/f_XmlEqual_r.xml"
    WORKING_DIRECTORY ${sample_dir})

generate_simple_comparison_test("XmlEqual" 1
    "" "${bindir}/f_XmlEqual_r.xml" "xml")

set_fixture_with_dep("f_XmlEqual")

set_tests_properties(f_XmlWellForm_Prep  PROPERTIES FIXTURES_SETUP   "F_XMLWELLFORM;F_XMLEQUAL")
set_tests_properties(f_XmlWellForm_Clean PROPERTIES FIXTURES_CLEANUP "F_XMLWELLFORM;F_XMLEQUAL")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        f_XmlDTDValidate
        f_XmlEqual
        f_XmlEqual_Clean
        f_XmlEqual_PrepAlt
        f_XmlEqual_Prep
        f_XmlWellForm
        f_XmlWellForm_Clean
        f_XmlWellForm_Prep
        PROPERTIES DISABLED true)
endif()

#
# $Recycle.bin counterpart for everything above
#

add_test(NAME d_XmlWellForm_Prep
    COMMAND rifiuti-vista -o ${bindir}/d_XmlWellForm.xml -x dir-sample1
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME d_XmlWellForm
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml)

add_test(NAME d_XmlDTDValidate
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME d_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_XmlWellForm.xml)

# d_XmlWellForm_Prep/Clean are used in multiple fixtures,
# properties set further down below
set_tests_properties(d_XmlWellForm    PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)
set_tests_properties(d_XmlDTDValidate PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)

set_tests_properties(
    d_XmlWellForm d_XmlDTDValidate
    PROPERTIES LABELS "recycledir;xml")


# XML equality test for $Recycle.bin

add_test_using_shell(d_XmlEqual_PrepAlt
    "${XMLLINT} --c14n d_XmlWellForm.xml > ${bindir}/d_XmlEqual.output")
set_tests_properties(d_XmlEqual_PrepAlt
    PROPERTIES DEPENDS d_XmlWellForm_Prep)

add_test_using_shell(d_XmlEqual_Prep
    "${XMLLINT} --c14n dir-sample1.xml > ${bindir}/d_XmlEqual_r.xml"
    WORKING_DIRECTORY ${sample_dir})

generate_simple_comparison_test("XmlEqual" 0
    "" "${bindir}/d_XmlEqual_r.xml" "xml")

set_fixture_with_dep("d_XmlEqual")

set_tests_properties(d_XmlWellForm_Prep  PROPERTIES FIXTURES_SETUP   "D_XMLWELLFORM;D_XMLEQUAL")
set_tests_properties(d_XmlWellForm_Clean PROPERTIES FIXTURES_CLEANUP "D_XMLWELLFORM;D_XMLEQUAL")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        d_XmlDTDValidate
        d_XmlEqual
        d_XmlEqual_Clean
        d_XmlEqual_PrepAlt
        d_XmlEqual_Prep
        d_XmlWellForm
        d_XmlWellForm_Clean
        d_XmlWellForm_Prep
        PROPERTIES DISABLED true)
endif()

