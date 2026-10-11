add_library(
	lib_init STATIC

		"src/init/init.c"
		"src/init/ld.c"
)
target_include_directories(
	lib_init

	PUBLIC
		"include/"
)
set_source_files_properties("src/init/ld.c" PROPERTIES COMPILE_FLAGS "-fno-lto")

zephyr_linker_sources(SECTIONS src/iterables.ld)
zephyr_linker_sources(DATA_SECTIONS src/callback_state_alloc.ld)

target_link_libraries(lib_init PRIVATE zephyr_interface)
add_dependencies(lib_init offsets_h)
