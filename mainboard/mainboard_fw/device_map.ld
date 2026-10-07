/* Fraise device memory map */
__settings_length__ = 4k;

MEMORY
{
    FRAISE_BLD(r)   : ORIGIN = 0x10000000 , LENGTH = 128k
    EEPROM(r)       : ORIGIN = ORIGIN(FRAISE_BLD) + LENGTH(FRAISE_BLD), LENGTH = 4k
    APP(rx)         : ORIGIN = ORIGIN(EEPROM) + LENGTH(EEPROM), LENGTH = 4096k - (ORIGIN(APP) - 0x10000000) - __settings_length__
    SETTINGS(r)     : ORIGIN = 0x10000000 + 4096k - __settings_length__, LENGTH = __settings_length__

    RAM(rwx)        : ORIGIN = 0x20000000, LENGTH = 512k
    SCRATCH_X(rwx)  : ORIGIN = 0x20040000, LENGTH = 4k
    SCRATCH_Y(rwx)  : ORIGIN = 0x20041000, LENGTH = 4k
}

__fraise_bootloader_start__ = ORIGIN(FRAISE_BLD);
__eeprom_start__ = ORIGIN(EEPROM);
__fraise_app_start__ = ORIGIN(APP);
__fraise_app_length__ = LENGTH(APP);
__settings_start__ = ORIGIN(SETTINGS);
