add_library(claim_pio INTERFACE)

target_sources(claim_pio INTERFACE 
	${CMAKE_CURRENT_LIST_DIR}/claim_pio.c
)

target_include_directories(claim_pio INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(claim_pio INTERFACE hardware_pio)

