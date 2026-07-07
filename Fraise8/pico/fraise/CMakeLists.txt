add_library(fraise INTERFACE)

target_sources(fraise INTERFACE 
	${CMAKE_CURRENT_LIST_DIR}/fraise.cpp
)

target_include_directories(fraise INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}
)
#target_link_libraries(fraise INTERFACE hardware_pio)

