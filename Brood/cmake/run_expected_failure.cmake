if(NOT DEFINED TEST_EXECUTABLE OR TEST_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "TEST_EXECUTABLE is required")
endif()

set(_test_args)
if(DEFINED TEST_ARGS AND NOT TEST_ARGS STREQUAL "")
    string(REPLACE "|" ";" _test_args "${TEST_ARGS}")
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}" ${_test_args}
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_stdout
    ERROR_VARIABLE test_stderr
)

if(test_result EQUAL 0)
    message(FATAL_ERROR
        "Expected command to fail but it succeeded.\n"
        "Command: ${TEST_EXECUTABLE} ${_test_args}\n"
        "stdout:\n${test_stdout}\n"
        "stderr:\n${test_stderr}\n")
endif()

if(DEFINED EXPECT_OUTPUT_REGEX AND NOT EXPECT_OUTPUT_REGEX STREQUAL "")
    set(_combined_output "${test_stdout}\n${test_stderr}")
    if(NOT _combined_output MATCHES "${EXPECT_OUTPUT_REGEX}")
        message(FATAL_ERROR
            "Expected output to match regex: ${EXPECT_OUTPUT_REGEX}\n"
            "stdout:\n${test_stdout}\n"
            "stderr:\n${test_stderr}\n")
    endif()
endif()
