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
    static constexpr int partition_table_size = primary_slots_count / 8;
    static constexpr int slots_count = (flash_size - partition_table_size) / slot_size;
private:
    using SP = SettingsPartition<slot_size, flash_size>;
    const char *flash_start;
    int current_slot = -1;

    bool partition_table_test_free(int slot) {
        return (flash_start[slot / 8] & (1 << (slot %8))) == 1;
    }

    void partition_table_cache_clear_free(char *cache, int slot) {
        cache[slot / 8] &= (0xff - (1 << (slot %8)));
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
        flash_range_erase(that->flash_start - XIP_BASE, flash_size);
    }

    static void call_flash_range_program(void *param) {
        char *dest = (char*)((uintptr_t*)param)[0];
        const char *source = (const char*)((uintptr_t*)param)[1];
        flash_range_program(dest - XIP_BASE, source, flash_size);
    }

    void read_from_flash(char *cache) {
        memcpy(cache, flash_start, flash_size);
    }

    void write_to_flash(char *cache) {
        uintptr_t params[] = {flash_start, cache};
        flash_safe_execute(call_flash_range_program, params, UINT32_MAX);
    }

public:
    SettingsPartition(const char *flash_start) : flash_start(flash_start) {
        current_slot = -1;
        // find last written slot
        for(int slot = 0; slot < slots_count && !partition_table_test_free(slot); slot++) current_slot++;
    }

    bool read(char *destination) {
        if(current_slot = -1) return false;
        memcpy(destination, flash_start + get_slot_index(current_slot), slot_size);
        return true;
    }

    void write(char *source) {
        char cache[flash_size];
        current_slot = current_slot + 1;
        read_from_flash(cache);
        while(current_slot < slots_count && !(partition_table_test_free(current_slot) && verify_fresh(current_slot))) {
            partition_table_cache_clear_free(cache, current_slot);
            current_slot++;
        }
        if(current_slot == slots_count) { // partition is full! erase it.
            erase_all();
            current_slot = 0;
        }
        memcpy(cache + get_slot_index(current_slot), source, slot_size);
        partition_table_cache_clear_free(cache, current_slot);
        write_to_flash(cache);
        fraise_printf("settings partition: wrote slot %d\n", current_slot);
    }

    void erase_all() {
        flash_safe_execute(call_flash_range_erase, this, UINT32_MAX);
        current_slot = -1;
        fraise_printf("settings partition: erase all\n");
    }
};

