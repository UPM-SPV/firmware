cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
project(firmware)

add_subdirectory(lib)

target_sources(
	app

	PRIVATE
		src/main.c

		src/fs/fs.c
		src/fs/internal.c
)

target_include_directories(
	app

	PRIVATE
		src/
)

target_link_libraries(app PUBLIC zephyr_interface)

target_link_libraries(app PRIVATE lib_attest)
target_link_libraries(app PRIVATE lib_battery)
target_link_libraries(app PRIVATE lib_factory_data)
target_link_libraries(app PRIVATE lib_init)
target_link_libraries(app PRIVATE lib_mpsl)
target_link_libraries(app PRIVATE lib_rram)
target_link_libraries(app PRIVATE lib_timesync)
