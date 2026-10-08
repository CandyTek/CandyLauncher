# copy_if_different.cmake
# 接收参数：src (源目录), dst (目标目录)

if(NOT DEFINED src OR NOT DEFINED dst)
	message(FATAL_ERROR "src and dst must be defined! Usage: cmake -Dsrc=... -Ddst=... -P copy_if_different.cmake")
endif()

if(NOT EXISTS "${src}")
	message(WARNING "Source directory does not exist: ${src}")
	return()
endif()

if(NOT IS_DIRECTORY "${src}")
	message(WARNING "Source path is not a directory or does not exist: ${src}")
	return()
endif()

# 核心兼容判断
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
	# 3.26+ 原生支持目录级增量拷贝
	execute_process(
			COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${src}" "${dst}"
			RESULT_VARIABLE _res
	)
	if(NOT _res EQUAL 0)
		message(FATAL_ERROR "Failed to copy directory from ${src} to ${dst}")
	endif()
else()
	# 3.26 以下：递归增量拷贝每个文件
	file(MAKE_DIRECTORY "${dst}")
	file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${src}" "${src}/*")
	foreach(_f IN LISTS _files)
		file(RELATIVE_PATH _rel "${src}" "${_f}")
		get_filename_component(_sub "${_rel}" DIRECTORY)
		if(_sub)
			file(MAKE_DIRECTORY "${dst}/${_sub}")
		endif()
		execute_process(
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_f}" "${dst}/${_rel}"
		)
		if(NOT _f_res EQUAL 0)
			message(FATAL_ERROR "Failed to copy file: ${_rel}")
		endif()
	endforeach()
endif()
