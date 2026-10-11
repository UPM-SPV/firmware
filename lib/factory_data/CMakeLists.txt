add_library(
	lib_factory_data STATIC

		"src/factory_data/factory_data.c"
)
target_include_directories(
	lib_factory_data

	PUBLIC
		"include/"
)

zephyr_nanopb_sources(lib_factory_data ../../res/protobuf/factory_data.proto)

target_link_libraries(lib_factory_data PRIVATE zephyr_interface)
target_link_libraries(lib_factory_data PRIVATE lib_attest)
