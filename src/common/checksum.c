#include "checksum.h"

static uint32_t crc32_table[256];
static int table_initialized = 0;

static void init_crc32_table(void) {
    uint32_t polynomial = 0xEDB88320U;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    table_initialized = 1;
}

uint32_t crc32_calculate(const void *data, size_t len) {
    if (!table_initialized) {
        init_crc32_table();
    }

    const uint8_t *buffer = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = buffer[i];
        crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFFU];
    }

    return crc ^ 0xFFFFFFFFU;
}
