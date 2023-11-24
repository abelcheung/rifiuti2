# Copyright (C) 2023, Abel Cheung
# rifiuti2 is released under Revised BSD License.
# Please see LICENSE file for more info.

#
# Execute encoding tests as external cmake script
#
# 1. $CMAKE_CURRENT_BINARY_DIR etc in cmake script aren't
# stable; they would change into wherever WORKING_DIRECTORY
# is set, so program paths must be supplied externally.
# 2. CHOICES (encoding list) is supplied as '|' separated
# list, in order to not collide with semicolon handling
# in cmake.
#

string(REPLACE "|" ";" CHOICES "${CHOICES}")
list(LENGTH CHOICES len)

if(len EQUAL 1)
    set(encoding ${CHOICES})
else()
    execute_process(
        COMMAND ${TEST_GLIB_ICONV} ${CHOICES}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE encoding
        # COMMAND_ECHO STDOUT
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "No suitable encoding found in glib")
    endif()
endif()

set(args -l ${encoding} ${INFO2})
if(DEFINED OUTFILE)
    list(APPEND args -o ${OUTFILE})
endif()
execute_process(
    COMMAND ${RIFIUTI} ${args}
    # COMMAND_ECHO STDOUT
    ERROR_VARIABLE err_result
    ECHO_ERROR_VARIABLE)
