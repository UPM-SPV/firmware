add_library(
	lib_rram STATIC

		"src/rram/rram.c"
)
target_include_directories(
	lib_rram

	PUBLIC
		"include/"
)

target_link_libraries(lib_rram PUBLIC zephyr_interface)
add_dependencies(lib_rram offsets_h)
