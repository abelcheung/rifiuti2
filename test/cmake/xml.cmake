# Search xmllint

add_test(
    NAME FindCommand_xmllint
    COMMAND xmllint --version
)

set_tests_properties(
    FindCommand_xmllint
    PROPERTIES
        FIXTURES_SETUP XMLLINT
)

# XML well-formed validation for INFO2

add_test(
    NAME f_CreateXmlOutput
    COMMAND rifiuti -o f_sample1.xml -x samples/INFO2-sample1
)

add_test(
    NAME f_CleanXmlOutput
    COMMAND ${CMAKE_COMMAND} -E remove f_sample1.xml
)

add_test(
    NAME f_XmlValidate
    COMMAND xmllint --noout f_sample1.xml
)

set_tests_properties(
    f_CreateXmlOutput
    PROPERTIES
        FIXTURES_SETUP F_XMLOUTPUT
)

set_tests_properties(
    f_CleanXmlOutput
    PROPERTIES
        FIXTURES_CLEANUP F_XMLOUTPUT
)

set_tests_properties(
    f_XmlValidate
    PROPERTIES
        FIXTURES_REQUIRED "XMLLINT;F_XMLOUTPUT"
)

# XML well-formed validation for $Recycle.bin

add_test(
    NAME d_CreateXmlOutput
    COMMAND rifiuti-vista -o d_sample1.xml -x samples/dir-sample1
)

add_test(
    NAME d_CleanXmlOutput
    COMMAND ${CMAKE_COMMAND} -E remove d_sample1.xml
)

add_test(
    NAME d_XmlValidate
    COMMAND xmllint --noout d_sample1.xml
)

set_tests_properties(
    d_CreateXmlOutput
    PROPERTIES
        FIXTURES_SETUP D_XMLOUTPUT
)

set_tests_properties(
    d_CleanXmlOutput
    PROPERTIES
        FIXTURES_CLEANUP D_XMLOUTPUT
)
set_tests_properties(
    d_XmlValidate
    PROPERTIES
        FIXTURES_REQUIRED "XMLLINT;D_XMLOUTPUT"
)

# DTD validation for INFO2

add_test(
    NAME f_DTDValidate
    COMMAND xmllint --noout f_sample1.xml --dtdvalid ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd
)

set_tests_properties(
    f_DTDValidate
    PROPERTIES
        FIXTURES_REQUIRED "XMLLINT;F_XMLOUTPUT"
)

# DTD validation for $Recycle.bin

add_test(
    NAME d_DTDValidate
    COMMAND xmllint --noout d_sample1.xml --dtdvalid ${CMAKE_CURRENT_SOURCE_DIR}/rifiuti.dtd
)

set_tests_properties(
    d_DTDValidate
    PROPERTIES
        FIXTURES_REQUIRED "XMLLINT;D_XMLOUTPUT"
)

# Other properties

set(
    f_XmlTests
    f_DTDValidate
    f_XmlValidate
)

set(
    d_XmlTests
    d_DTDValidate
    d_XmlValidate
)

set_tests_properties(
    ${d_XmlTests}
    PROPERTIES
        LABELS "recycledir;xml"
)

set_tests_properties(
    ${f_XmlTests}
    PROPERTIES
        LABELS "info2;xml"
)
