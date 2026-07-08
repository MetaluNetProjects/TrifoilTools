add_library(fraise_bus INTERFACE)

target_sources(fraise_bus INTERFACE 
	${CMAKE_CURRENT_LIST_DIR}/fraise_uart.cpp
	${CMAKE_CURRENT_LIST_DIR}/fraise_poller.cpp
	${CMAKE_CURRENT_LIST_DIR}/fraise_bus.cpp
)

target_include_directories(fraise_bus INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fraise_bus INTERFACE
	pico_stdlib
	#pico_sync
	hardware_uart
	fraise
)

