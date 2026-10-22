# Check malformed synthetic FILETIME handling without reading any other index data.

foreach(required_var PROGRAM INPUT FORMAT INDEX)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "Missing required variable ${required_var}")
    endif()
endforeach()

execute_process(
    COMMAND "${PROGRAM}" -f "${FORMAT}" "${INPUT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT "${result}" STREQUAL "5")
    message(FATAL_ERROR "Expected record-error exit code 5, got '${result}'\n${error}")
endif()

if("${error}" MATCHES "GLib-CRITICAL")
    message(FATAL_ERROR "Invalid FILETIME triggered a GLib critical\n${error}")
endif()

string(FIND "${error}" "File deletion time is suspicious or broken" error_pos)
if(error_pos EQUAL -1)
    message(FATAL_ERROR "Invalid FILETIME was not reported as a record error\n${error}")
endif()

if(FORMAT STREQUAL "text")
    string(ASCII 9 tab)
    set(time_marker "${INDEX}${tab}???${tab}")
elseif(FORMAT STREQUAL "xml")
    set(time_marker "time=\"???\"")
elseif(FORMAT STREQUAL "json")
    set(time_marker "\"time\": \"???\"")
else()
    message(FATAL_ERROR "Unsupported output format '${FORMAT}'")
endif()

string(FIND "${output}" "${time_marker}" time_pos)
if(time_pos EQUAL -1)
    message(FATAL_ERROR "Unknown timestamp marker or incomplete record output\n${output}")
endif()

foreach(field "123" "synthetic.txt")
    string(FIND "${output}" "${field}" field_pos)
    if(field_pos EQUAL -1)
        message(FATAL_ERROR "Record output is missing '${field}'\n${output}")
    endif()
endforeach()
