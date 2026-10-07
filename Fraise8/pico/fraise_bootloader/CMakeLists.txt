project(bootloader C CXX ASM)

add_executable(bootloader main.cpp bootloader.cpp)

pico_add_extra_outputs(bootloader)

#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Wall")
#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES COMPILE_OPTIONS "-DFRAISE_BLD_DEBUG")

# get every asm intermediate output
#target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -save-temps -fverbose-asm)

target_link_libraries(bootloader
    pico_stdlib
    hardware_irq
    hardware_watchdog
    hardware_structs
    hardware_resets
    hardware_sync
    hardware_flash
    #pico_unique_id
    cmsis_core
    #fraise_eeprom
    fraise_bus
)

pico_enable_stdio_usb(bootloader 0)
pico_enable_stdio_uart(bootloader 0)

pico_set_float_implementation(bootloader none)
pico_set_double_implementation(bootloader none)

