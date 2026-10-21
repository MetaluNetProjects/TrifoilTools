/* Fraise master memory map */

__settings_length__ = 4k;
__flash_size__ = 2048k;

MEMORY
{
    USB_BLD(r)      : ORIGIN = 0x10000000, LENGTH = 32k
    EEPROM(r)       : ORIGIN = ORIGIN(USB_BLD) + LENGTH(USB_BLD), LENGTH = 4k
    APP(rx)         : ORIGIN = ORIGIN(EEPROM) + LENGTH(EEPROM), LENGTH = __flash_size__ - (LENGTH(USB_BLD) + LENGTH(EEPROM)) - __settings_length__
    SETTINGS(r)     : ORIGIN = 0x10000000 + __flash_size__ - __settings_length__, LENGTH = __settings_length__
    RAM(rwx)        : ORIGIN = 0x20000000, LENGTH = 256k
    SCRATCH_X(rwx)  : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx)  : ORIGIN = 0x20041000, LENGTH = 4k
}

__usb_bootloader_start__ = ORIGIN(USB_BLD);
__eeprom_start__ = ORIGIN(EEPROM);
__fraise_app_start__ = ORIGIN(APP);
__fraise_app_length__ = LENGTH(APP);
__settings_start__ = ORIGIN(SETTINGS);
