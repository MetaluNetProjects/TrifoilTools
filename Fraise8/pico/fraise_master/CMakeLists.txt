add_library(fraise_master INTERFACE)

target_sources(fraise_master INTERFACE 
    fraise_master.cpp
)

target_include_directories(fraise_master INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(fraise_master INTERFACE
    pico_stdlib
    hardware_irq
    hardware_watchdog
    hardware_structs
    pico_async_context_threadsafe_background
    pico_async_context_poll
    cmsis_core
    fraise_eeprom
    fraise
    fraise_bus
)

# set_target_properties(fraise_device PROPERTIES INTERFACE_COMPILE_OPTIONS "-DFRAISE_MASTER_DEBUG")

