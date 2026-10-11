cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# This compiler directive is required to compile Zephyr 4.4.2 under C23.
#
# Currently, some internal Zephyr APIs assume empty parameter lists in function
# declarations mean an unspecified number of arguments. However, C23 treats
# them as equivalent to a (void) parameter list [void fn(void) == void fn()].
#
# The only API erroring while building this repo is the logging subsystem:
# zephyr/subsys/logging/log_output.c. However, others may arise during
# development.
#
# FIXME[c23]: Remove this when Zephyr bundles the relevant fix into a final release,
# either 4.4.3 or 4.5.0, and Nordic pulls it into NCS.
#
# Commit implementing the fix:
# 	https://github.com/zephyrproject-rtos/zephyr/commit/f3d287cace40ff727347c32b0ec51bc334b34cbb
zephyr_compile_options(-Wno-error=incompatible-pointer-types)


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
		src/ble/attestation.c
		src/ble/battery.c
		src/ble/dis.c
		src/ble/environment.c
		src/ble/l2cap.c
		src/ble/settings.c
		src/ble/sensor.c
		src/ble/time.c

		src/fs/fs.c
		src/fs/internal.c

		src/init/attest.c
		src/init/battery.c
		src/init/ble.c
		src/init/factorydata.c
		src/init/fs.c
		src/init/mpsl.c
		src/init/sensors.c
		src/init/settings.c
		src/init/task_button_led.c
		src/init/task_sensors.c
		src/init/timesync.c

		src/sense/im70d122.c
		src/sense/sgp40.c
		src/sense/shtc3.c
		src/sense/veml6035.c

		src/settings/settings.c

		src/tasks/button_led.c
		src/tasks/sensors.c
)
# FIXME[c23]: Remove this when C23 support is fixed as well.
set_property(TARGET app PROPERTY C_STANDARD 23)

target_include_directories(
	app

	PRIVATE
		src/
)

zephyr_nanopb_sources(app res/protobuf/device_settings.proto)
zephyr_nanopb_sources(app res/protobuf/sensor_data_request.proto)

zephyr_linker_sources(SECTIONS src/iterables.ld)

target_link_libraries(app PRIVATE lib_attest)
target_link_libraries(app PRIVATE lib_battery)
target_link_libraries(app PRIVATE lib_factory_data)
target_link_libraries(app PRIVATE lib_init)
target_link_libraries(app PRIVATE lib_mpsl)
target_link_libraries(app PRIVATE lib_rram)
target_link_libraries(app PRIVATE lib_timesync)
