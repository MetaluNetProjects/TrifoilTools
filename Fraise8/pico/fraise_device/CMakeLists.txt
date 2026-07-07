add_library(fraise_device INTERFACE)

target_sources(fraise_device INTERFACE 
    fraise_device.cpp
)

target_include_directories(fraise_device INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(fraise_device INTERFACE
    pico_stdlib
    hardware_irq
    hardware_watchdog
    hardware_structs
    #pico_async_context_threadsafe_background
    #pico_async_context_poll
    cmsis_core
    fraise_eeprom
    fraise
    fraise_bus
)

