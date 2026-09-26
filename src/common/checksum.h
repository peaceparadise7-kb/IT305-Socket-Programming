#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>

/*
 * Calculates IEEE 802.3 CRC32 checksum for given binary buffer.
 * Test Vector: crc32_calculate("123456789", 9) == 0xCBF43926U
 */
uint32_t crc32_calculate(const void *data, size_t len);

#endif /* CHECKSUM_H */
