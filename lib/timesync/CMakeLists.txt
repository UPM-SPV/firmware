add_library(
	lib_timesync STATIC

		"src/timesync/timesync.c"
)
target_include_directories(
	lib_timesync

	PUBLIC
		"include/"
)
zephyr_linker_sources(SECTIONS src/iterables.ld)

target_link_libraries(lib_timesync PRIVATE zephyr_interface)
target_link_libraries(lib_timesync PRIVATE lib_factory_data)
