add_library(
	lib_mpsl STATIC

		"src/mpsl/mpsl.c"
)
target_include_directories(
	lib_mpsl

	PUBLIC
		"include/"
)

zephyr_linker_sources(SECTIONS src/iterables.ld)

target_link_libraries(lib_mpsl PRIVATE zephyr_interface)
