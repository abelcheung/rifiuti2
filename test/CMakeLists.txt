include(CTest)

set(
    CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
)

file(
    CREATE_LINK
    ${CMAKE_CURRENT_SOURCE_DIR}/samples
    ${CMAKE_CURRENT_BINARY_DIR}/samples
    SYMBOLIC
)

include(cli-option)
include(crafted)
include(xml)
