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

		src/ble/ble.c
		src/ble/advertise.c
		src/ble/battery.c
		src/ble/dis.c
		src/ble/settings.c
		src/ble/time.c

		src/fs/fs.c
		src/fs/internal.c

		src/sense/im70d122.c
		src/sense/sgp40.c
		src/sense/shtc3.c
		src/sense/veml6035.c

		src/settings/settings.c
)

target_include_directories(
	app

	PRIVATE
		src/
)

zephyr_nanopb_sources(app res/protobuf/device_settings.proto)

zephyr_linker_sources(SECTIONS src/iterables.ld)
target_link_libraries(app PUBLIC zephyr_interface)
add_dependencies(app nanopb_generated_headers)

target_link_libraries(app PRIVATE lib_attest)
target_link_libraries(app PRIVATE lib_battery)
target_link_libraries(app PRIVATE lib_factory_data)
target_link_libraries(app PRIVATE lib_init)
target_link_libraries(app PRIVATE lib_mpsl)
target_link_libraries(app PRIVATE lib_rram)
target_link_libraries(app PRIVATE lib_timesync)
