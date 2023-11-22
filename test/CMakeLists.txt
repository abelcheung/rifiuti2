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

function(add_test_using_shell name command)
    if(WIN32)
        add_test(
            NAME ${name}
            COMMAND pwsh -NonInteractive -NoProfile -Command "${command}"
            ${ARGN})
    else()
        add_test(
            NAME ${name}
            COMMAND bash -c "${command}"
            ${ARGN})
    endif()
endfunction()

include(cli-option)
include(crafted)
include(read-write)
include(xml)
