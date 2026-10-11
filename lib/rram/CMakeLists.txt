add_library(
	lib_rram STATIC

		"src/rram/rram.c"
)
target_include_directories(
	lib_rram

	PUBLIC
		"include/"
)

target_link_libraries(lib_rram PRIVATE zephyr_interface)
