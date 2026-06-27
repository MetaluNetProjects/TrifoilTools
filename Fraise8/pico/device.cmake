# to be invoked from ${projDir}/build with:
# cmake -S . -B  . -Dfraise_path=path/to/Fraise -is_pied_fruit=[0/1]
cmake_minimum_required(VERSION 3.13)

# get source files
file(GLOB_RECURSE srcfiles FOLLOW_SYMLINKS CONFIGURE_DEPENDS ${projDir}/*.c ${projDir}/*.cpp ${projDir}/*.cc)
list(FILTER srcfiles EXCLUDE REGEX "build.*/*|board/*")
message("source files recursive: ${srcfiles}")
add_executable(${projName} ${srcfiles})

message("board: ${FRAISE_BOARD}")
message("PICO_BOARD: ${PICO_BOARD}")
message("PICO_PLATFORM: ${PICO_PLATFORM}")

target_compile_definitions(${projName} PUBLIC FRAISE_RX_PIN=${FRAISE_RX_PIN})
target_compile_definitions(${projName} PUBLIC FRAISE_TX_PIN=${FRAISE_TX_PIN})
target_compile_definitions(${projName} PUBLIC FRAISE_DRV_PIN=${FRAISE_DRV_PIN})
target_compile_definitions(${projName} PUBLIC FRAISE_DRV_LEVEL=${FRAISE_DRV_LEVEL})
target_compile_definitions(${projName} PUBLIC FRAISE_ID=${FRAISE_ID})

# add pico modules
add_subdirectory("${fraise_path}/pico/" modules)

pico_add_extra_outputs(${projName})

target_link_libraries(${projName} pico_stdlib)

pico_enable_stdio_usb(${projName} 0)
pico_enable_stdio_uart(${projName} 0)

if(DEFINED ENV{is_pied_fruit})
    set(is_pied_fruit $ENV{is_pied_fruit})
endif()

message("is_pied_fruit = ${is_pied_fruit}")

# linking script:

if(${PICO_PLATFORM} STREQUAL rp2350-arm-s)
    set(ld_scripts_path ${fraise_path}/pico/rp2350)
else()
    set(ld_scripts_path ${fraise_path}/pico/rp2040)
endif()
message("ld_scripts_path: ${ld_scripts_path}")

target_link_options(${projName} PUBLIC "-L${ld_scripts_path}")

if(${is_pied_fruit} EQUAL 1)
    pico_set_linker_script(${projName} "${ld_scripts_path}/master_app.ld")
    target_link_libraries(${projName} fraise fraise_master)
    pico_enable_stdio_usb(${projName} 1)
    add_subdirectory(${fraise_path}/pico/usb_bootloader bootloader)
    set(APP_BIN ${CMAKE_CURRENT_BINARY_DIR}/${projName}.bin)
    set(BLD_ELF ${CMAKE_CURRENT_BINARY_DIR}/bootloader/bootloader.elf)
    add_custom_target(usb_bootloader ALL
        DEPENDS ${projName} bootloader
        COMMAND ${CMAKE_OBJCOPY} --update-section .app_bin=${APP_BIN} ${BLD_ELF} usb_bootloader.elf
        COMMAND picotool uf2 convert usb_bootloader.elf usb_bootloader.uf2
    )
else()
    pico_set_linker_script(${projName} "${ld_scripts_path}/device_app.ld")
    target_link_libraries(${projName} fraise fraise_device)
    add_subdirectory(${fraise_path}/pico/fraise_bootloader bootloader)
    set(APP_BIN ${CMAKE_CURRENT_BINARY_DIR}/${projName}.bin)
    set(BLD_ELF ${CMAKE_CURRENT_BINARY_DIR}/bootloader/bootloader.elf)
    add_custom_target(fraise_bootloader ALL
        DEPENDS ${projName} bootloader
        COMMAND ${CMAKE_OBJCOPY} --update-section .app_bin=${APP_BIN} ${BLD_ELF} fraise_bootloader.elf
        COMMAND picotool uf2 convert fraise_bootloader.elf fraise_bootloader.uf2
    )
endif()

if(${is_pied_fruit} EQUAL 1)
	set(target_file ${projName}-0.hex)
else()
	set(target_file ${projName}.hex)
endif()

add_custom_command(
	TARGET ${projName} POST_BUILD
	COMMENT "-- Calculating memory usage"
	COMMAND ${size_command} -G ${projName}.elf > size.txt
)

if (NOT DEFINED no_hex_copy)
add_custom_command(
	TARGET ${projName} POST_BUILD
	# COMMENT "-- Calculating memory usage"
	# COMMAND ${size_command} -G ${projName}.elf > size.txt
	COMMENT "-- Copying hex file to source directory"
	COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/${projName}.hex" "${PROJECT_SOURCE_DIR}/../${target_file}"
)
endif()

if(EXISTS "${projDir}/config.cmake")
	message("including custom config.cmake")
	include("${projDir}/config.cmake")
endif()

set_target_properties(${projName} PROPERTIES COMPILE_FLAGS "-Wall")
