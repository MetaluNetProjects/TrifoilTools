target_link_libraries(${CMAKE_PROJECT_NAME} fraise_uart_native)

set(USB_DEBUG true)
if(${USB_DEBUG})
    pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
    pico_enable_stdio_usb(bootloader 1)
    set_target_properties(bootloader PROPERTIES COMPILE_OPTIONS "-DFRAISE_BLD_DEBUG")
endif()

