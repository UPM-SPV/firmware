add_library(
	lib_attest STATIC

		"src/attest/attest.c"
)
target_include_directories(
	lib_attest

	PUBLIC
		"include/"
)

target_link_libraries(lib_attest PRIVATE zephyr_interface)
