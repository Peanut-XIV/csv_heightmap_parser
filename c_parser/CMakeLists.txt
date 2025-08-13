cmake_minimum_required(VERSION 3.10)
project(c_parser VERSION 1.0.0 LANGUAGES C)

add_executable(
	enduser_exec
	src/c_parser.c
	src/arg_parse.c
	include/arg_parse.h
	include/custom_dtypes.h
)

add_executable(
	test_file_identificator

	test/test_file_identificator.c

	src/file_identificator.c
	include/file_identificator.h

	include/ANSI_colors.h

	include/custom_dtypes.h
)

add_executable(
	benchmark

	src/benchmark.c

	src/arg_parse.c
	include/arg_parse.h

	src/file_identificator.c
	include/file_identificator.h

	src/buffer_util.c
	include/buffer_util.h

	src/utils.c
	include/utils.h

	include/custom_dtypes.h
)

add_executable(print_info src/print_info.c)

if(WIN32 AND CMAKE_HOST_UNIX)
	message(
		"The project will be compiled with mingw and will "
		"use the following compiler: ${LOCAL_MINGW_COMPILER}"
	)
	set(CMAKE_C_COMPILER ${LOCAL_MINGW_COMPILER})
	set(MINGW TRUE)
endif()

if(MSVC)
	target_compile_options(benchmark PRIVATE /W4 /WX)
else()
	target_compile_options(benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set_property(TARGET enduser_exec PROPERTY C_STANDARD 11)
set_property(TARGET enduser_exec PROPERTY C_STANDARD_REQUIRED 11)
