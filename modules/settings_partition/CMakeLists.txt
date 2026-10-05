add_library(settings_partition INTERFACE)

target_include_directories(settings_partition INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(settings_partition INTERFACE hardware_flash)

