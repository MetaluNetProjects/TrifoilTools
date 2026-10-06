// Settings partition

#pragma once
#include "fraise.hpp"
#include "string.h"
#include <hardware/flash.h>
#include "pico/flash.h"

template<int slot_size, int flash_size = 4096>
class SettingsPartition {
public:
    static constexpr int primary_slots_count = flash_size / slot_size;
    static constexpr int partition_table_size = ceil(primary_slots_count / 8.0f);
    static constexpr int slots_count = (flash_size - partition_table_size) / slot_size;
private:
    using SP = SettingsPartition<slot_size, flash_size>;
    const char *flash_start;
    int current_slot = -1;
    bool debug = false;

    bool partition_table_test_free(int slot) {
        return (flash_start[slot / 8] & (1 << (slot % 8))) != 0;
    }

    void partition_table_cache_clear_free(char *cache, int slot) {
        cache[slot / 8] &= (0xff - (1 << (slot % 8)));
    }

    int get_slot_index(int slot) {
        return slot_size * slot + partition_table_size;
    }

    bool verify_fresh(int slot) {
        int index = get_slot_index(slot);
        for(int i = 0; i < slot_size; i++) {
            if(flash_start[index + i] != 0xff) return false;
        }
        return true;
    }

    static void call_flash_range_erase(void *param) {
        SP *that = (SP*)param;
        flash_range_erase((uint32_t)(that->flash_start - XIP_BASE), flash_size);
    }

    static void call_flash_range_program(void *param) {
        char *dest = (char*)((uintptr_t*)param)[0];
        const char *source = (const char*)((uintptr_t*)param)[1];
        flash_range_program((uint32_t)(dest - XIP_BASE), (const uint8_t*)source, flash_size);
    }

    void read_from_flash(char *cache) {
        memcpy(cache, flash_start, flash_size);
    }

    void write_to_flash(char *cache) {
        const char* (params)[] = {flash_start, cache};
        int res = flash_safe_execute(call_flash_range_program, params, UINT32_MAX);
        if(debug) {
            if(res == PICO_OK) fraise_printf("settings write_to_flash OK\n");
            else if(res == PICO_ERROR_TIMEOUT) fraise_printf("settings write_to_flash TIMEOUT\n");
            else if(res == PICO_ERROR_NOT_PERMITTED) fraise_printf("settings write_to_flash NOT_PERMITTED\n");
            else if(res == PICO_ERROR_INSUFFICIENT_RESOURCES) fraise_printf("settings write_to_flash INSUFFICIENT_RESOURCES\n");
        }
    }

public:
    SettingsPartition(const char *flash_start) : flash_start(flash_start) {
        current_slot = -1;
        // find last written slot
        for(int slot = 0; slot < slots_count && !partition_table_test_free(slot); slot++) current_slot++;
    }

    bool read(char *destination, int max_bytes) {
        if(current_slot == -1) return false;
        if(debug) fraise_printf("settings partition: reading slot %d / %d\n", current_slot, slots_count);
        memcpy(destination, flash_start + get_slot_index(current_slot), MIN(slot_size, max_bytes));
        return true;
    }

    void write(char *source, int max_bytes) {
        char cache[flash_size];
        current_slot = current_slot + 1;
        read_from_flash(cache);
        while(current_slot < slots_count && !(partition_table_test_free(current_slot) && verify_fresh(current_slot))) {
            partition_table_cache_clear_free(cache, current_slot);
            current_slot++;
        }
        if(current_slot == slots_count) { // partition is full! erase it.
            if(debug) fraise_printf("settings partition write: partition is full\n");
            erase_all();
            read_from_flash(cache);
            current_slot = 0;
        }
        memcpy(cache + get_slot_index(current_slot), source, MIN(slot_size, max_bytes));
        partition_table_cache_clear_free(cache, current_slot);
        write_to_flash(cache);
        if(debug) fraise_printf("settings partition: wrote slot %d @ %d\n", current_slot, get_slot_index(current_slot));
    }

    void erase_all() {
        int res = flash_safe_execute(call_flash_range_erase, this, UINT32_MAX);
        if(debug) {
            if(res == PICO_OK) fraise_printf("settings erase OK\n");
            else if(res == PICO_ERROR_TIMEOUT) fraise_printf("settings erase TIMEOUT\n");
            else if(res == PICO_ERROR_NOT_PERMITTED) fraise_printf("settings erase NOT_PERMITTED\n");
            else if(res == PICO_ERROR_INSUFFICIENT_RESOURCES) fraise_printf("settings erase INSUFFICIENT_RESOURCES\n");
        }
        current_slot = -1;
        fraise_printf("settings partition: erased all\n");
    }

    char read_raw(int index) {
        return flash_start[index];
    }

    void set_debug(bool d) {
        debug = d;
    }
};

