# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

set(myDTD ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd)
set(sample_dir ${CMAKE_CURRENT_SOURCE_DIR}/samples)

# XML well-formed and DTD validation for INFO2

add_test(NAME f_XmlWellForm_Prep
    COMMAND rifiuti -o f_XmlWellForm.xml -x ${sample_dir}/INFO2-sample1)

add_test(NAME f_XmlWellForm
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml)

add_test(NAME f_XmlDTDValidate
    COMMAND ${XMLLINT} --noout f_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME f_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E remove f_XmlWellForm.xml)

set_tests_properties(f_XmlWellForm_Prep  PROPERTIES FIXTURES_SETUP    F_XMLWELLFORM)
set_tests_properties(f_XmlWellForm_Clean PROPERTIES FIXTURES_CLEANUP  F_XMLWELLFORM)
set_tests_properties(f_XmlWellForm       PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)
set_tests_properties(f_XmlDTDValidate    PROPERTIES FIXTURES_REQUIRED F_XMLWELLFORM)
set_tests_properties(
    f_XmlWellForm f_XmlDTDValidate
    PROPERTIES LABELS "info2;xml")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        f_XmlDTDValidate
        f_XmlWellForm
        f_XmlWellForm_Prep
        f_XmlWellForm_Clean
        PROPERTIES DISABLED true)
endif()

# XML well-formed and DTD validation for $Recycle.bin

add_test(NAME d_XmlWellForm_Prep
    COMMAND rifiuti-vista -o d_XmlWellForm.xml -x ${sample_dir}/dir-sample1)

add_test(NAME d_XmlWellForm
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml)

add_test(NAME d_XmlDTDValidate
    COMMAND ${XMLLINT} --noout d_XmlWellForm.xml --dtdvalid ${myDTD})

add_test(NAME d_XmlWellForm_Clean
    COMMAND ${CMAKE_COMMAND} -E remove d_XmlWellForm.xml)

set_tests_properties(d_XmlWellForm_Prep  PROPERTIES FIXTURES_SETUP    D_XMLWELLFORM)
set_tests_properties(d_XmlWellForm_Clean PROPERTIES FIXTURES_CLEANUP  D_XMLWELLFORM)
set_tests_properties(d_XmlWellForm       PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)
set_tests_properties(d_XmlDTDValidate    PROPERTIES FIXTURES_REQUIRED D_XMLWELLFORM)
set_tests_properties(
    d_XmlWellForm d_XmlDTDValidate
    PROPERTIES LABELS "recycledir;xml")

if("${XMLLINT}" STREQUAL "XMLLINT-NOTFOUND")
    set_tests_properties(
        d_XmlDTDValidate
        d_XmlWellForm
        d_XmlWellForm_Prep
        d_XmlWellForm_Clean
        PROPERTIES DISABLED true)
endif()

