# Generates OneNoteLib.tlh from the OneNote type library.
# Runs as a custom command so the header is only rewritten when the stub changes;
# a direct #import rewrites the .tlh on every compile and makes ninja rebuild forever.
# Inputs: COMPILER, SRC, WORK_DIR, OUT
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
execute_process(
		COMMAND "${COMPILER}" /nologo /c /EHsc "${SRC}" "/Fo${WORK_DIR}/"
		WORKING_DIRECTORY "${WORK_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE output
)
if (NOT result EQUAL 0)
	message(FATAL_ERROR "Failed to import OneNote type library:\n${output}")
endif ()
file(GLOB tlh_files "${WORK_DIR}/*.tlh")
list(LENGTH tlh_files tlh_count)
if (NOT tlh_count EQUAL 1)
	message(FATAL_ERROR "Expected one .tlh in ${WORK_DIR}, found: ${tlh_files}")
endif ()
configure_file("${tlh_files}" "${OUT}" COPYONLY)
