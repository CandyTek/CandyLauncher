# copy_if_missing_or_updated.cmake
# 传入：src, dst
file(GLOB_RECURSE _files LIST_DIRECTORIES false "${src}/*")
foreach(f IN LISTS _files)
	file(RELATIVE_PATH rel "${src}" "${f}")
	get_filename_component(subdir "${rel}" DIRECTORY)
	file(MAKE_DIRECTORY "${dst}/${subdir}")
	execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${f}" "${dst}/${rel}")
endforeach()
