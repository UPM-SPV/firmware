add_library(
	lib_battery STATIC

		"src/battery/battery.c"
)
target_include_directories(
	lib_battery

	PUBLIC
		"include/"
)
target_link_libraries(lib_battery PUBLIC zephyr_interface)
add_dependencies(lib_battery offsets_h)
