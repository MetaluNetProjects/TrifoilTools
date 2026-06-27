add_library(fraise_uart_native INTERFACE)

target_sources(fraise_uart_native INTERFACE 
	${CMAKE_CURRENT_LIST_DIR}/fraise_uart.c
)

target_include_directories(fraise_uart_native INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(fraise_uart_native INTERFACE
	pico_stdlib
	#pico_sync
	hardware_uart
)

