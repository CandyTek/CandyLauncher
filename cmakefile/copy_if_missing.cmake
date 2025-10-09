# cmake/copy_if_missing.cmake
# 传入变量：src, dst
if (NOT EXISTS "${dst}")
	message(STATUS "Copying directory: ${src} -> ${dst}")
	file(COPY "${src}/" DESTINATION "${dst}")
#else ()
#	message(STATUS "Skip copy: already exists: ${dst}")
endif ()
