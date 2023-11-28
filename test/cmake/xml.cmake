# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

set(myDTD ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd)
set(sample_dir ${CMAKE_CURRENT_SOURCE_DIR}/samples)
set(bindir ${CMAKE_CURRENT_BINARY_DIR})

# XML well-formed and DTD validation for INFO2

add_test(NAME f_XmlWellForm_Prep
    COMMAND rifiuti -o ${bindir}/f_XmlWellForm.xml -x INFO2-sample1
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME f_XmlWellForm
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml)

add_test(NAME f_XmlDTDValidate
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME f_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E remove f_XmlWellForm.xml)

# f_XmlWellForm_Prep/Clean are used in multiple fixtures,
# properties set further down below
set_tests_properties(f_XmlWellForm       PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)
set_tests_properties(f_XmlDTDValidate    PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)

# XML equality test for INFO2

# xmllint has a long known history that its --output option is broken.
# Have to use redirection instead.
# https://unix.stackexchange.com/q/492116

add_test_using_shell(f_XmlEqual_Prep1
    "${XMLLINT} --c14n f_XmlWellForm.xml > f_XmlEqual_o.xml")

add_test_using_shell(f_XmlEqual_Prep2
    "${XMLLINT} --c14n INFO2-sample1.xml > ${bindir}/f_XmlEqual_r.xml"
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME f_XmlEqual
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol f_XmlEqual_o.xml f_XmlEqual_r.xml)

add_test(NAME f_XmlEqual_Clean
    COMMAND ${CMAKE_COMMAND} -E rm f_XmlEqual_o.xml f_XmlEqual_r.xml)

set_tests_properties(
    f_XmlEqual_Prep1 f_XmlEqual_Prep2
    PROPERTIES FIXTURES_SETUP    F_XMLEQUAL)
set_tests_properties(f_XmlEqual_Clean PROPERTIES FIXTURES_CLEANUP  F_XMLEQUAL)
set_tests_properties(f_XmlEqual       PROPERTIES FIXTURES_REQUIRED F_XMLEQUAL)
set_tests_properties(f_XmlWellForm_Prep
    PROPERTIES FIXTURES_SETUP   "F_XMLWELLFORM;F_XMLEQUAL")
set_tests_properties(f_XmlWellForm_Clean
    PROPERTIES FIXTURES_CLEANUP "F_XMLWELLFORM;F_XMLEQUAL")
set_tests_properties(f_XmlEqual_Prep1 PROPERTIES DEPENDS f_XmlWellForm_Prep)

set_tests_properties(
    f_XmlWellForm f_XmlEqual f_XmlDTDValidate
    PROPERTIES LABELS "info2;xml")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        f_XmlDTDValidate
        f_XmlEqual
        f_XmlEqual_Clean
        f_XmlEqual_Prep1
        f_XmlEqual_Prep2
        f_XmlWellForm
        f_XmlWellForm_Clean
        f_XmlWellForm_Prep
        PROPERTIES DISABLED true)
endif()

# XML well-formed and DTD validation for $Recycle.bin

add_test(NAME d_XmlWellForm_Prep
    COMMAND rifiuti-vista -o ${bindir}/d_XmlWellForm.xml -x dir-sample1
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME d_XmlWellForm
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml)

add_test(NAME d_XmlDTDValidate
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME d_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E remove d_XmlWellForm.xml)

# d_XmlWellForm_Prep/Clean are used in multiple fixtures,
# properties set further down below
set_tests_properties(d_XmlWellForm       PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)
set_tests_properties(d_XmlDTDValidate    PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)

# XML equality test for $Recycle.bin

add_test_using_shell(d_XmlEqual_Prep1
    "${XMLLINT} --c14n d_XmlWellForm.xml > d_XmlEqual_o.xml")

add_test_using_shell(d_XmlEqual_Prep2
    "${XMLLINT} --c14n dir-sample1.xml > ${bindir}/d_XmlEqual_r.xml"
    WORKING_DIRECTORY ${sample_dir})

add_test(NAME d_XmlEqual
    COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol d_XmlEqual_o.xml d_XmlEqual_r.xml)

add_test(NAME d_XmlEqual_Clean
    COMMAND ${CMAKE_COMMAND} -E rm d_XmlEqual_o.xml d_XmlEqual_r.xml)

set_tests_properties(
    d_XmlEqual_Prep1 d_XmlEqual_Prep2
    PROPERTIES FIXTURES_SETUP    D_XMLEQUAL)
set_tests_properties(d_XmlEqual_Clean PROPERTIES FIXTURES_CLEANUP  D_XMLEQUAL)
set_tests_properties(d_XmlEqual       PROPERTIES FIXTURES_REQUIRED D_XMLEQUAL)
set_tests_properties(d_XmlWellForm_Prep
    PROPERTIES FIXTURES_SETUP   "D_XMLWELLFORM;D_XMLEQUAL")
set_tests_properties(d_XmlWellForm_Clean
    PROPERTIES FIXTURES_CLEANUP "D_XMLWELLFORM;D_XMLEQUAL")
set_tests_properties(d_XmlEqual_Prep1 PROPERTIES DEPENDS d_XmlWellForm_Prep)

set_tests_properties(
    d_XmlWellForm d_XmlEqual d_XmlDTDValidate
    PROPERTIES LABELS "recycledir;xml")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        d_XmlDTDValidate
        d_XmlEqual
        d_XmlEqual_Clean
        d_XmlEqual_Prep1
        d_XmlEqual_Prep2
        d_XmlWellForm
        d_XmlWellForm_Clean
        d_XmlWellForm_Prep
        PROPERTIES DISABLED true)
endif()

