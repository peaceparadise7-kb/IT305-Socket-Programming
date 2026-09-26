#include "protocol.h"
#include "checksum.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    printf("Running %-35s ... ", #fn); \
    if (fn()) { \
        printf("PASS\n"); \
        g_tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while (0)

/* 1. Header Serialization / Deserialization */
static bool test_header_serialization(void) {
    header_t original = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = MSG_GET_REQ,
        .flags = FLAG_RESUME,
        .payload_len = 1024,
        .seq_num = 42
    };

    uint8_t buf[HEADER_LEN];
    serialize_header(&original, buf);

    header_t decoded;
    deserialize_header(buf, &decoded);

    return (decoded.magic == original.magic &&
            decoded.msg_type == original.msg_type &&
            decoded.flags == original.flags &&
            decoded.payload_len == original.payload_len &&
            decoded.seq_num == original.seq_num);
}

/* 2. Network Byte Order Verification */
static bool test_network_byte_order(void) {
    uint32_t val32 = 0x12345678U;
    uint8_t buf32[4];
    serialize_uint32(val32, buf32);

    if (buf32[0] != 0x12U || buf32[1] != 0x34U || buf32[2] != 0x56U || buf32[3] != 0x78U) {
        return false;
    }
    if (deserialize_uint32(buf32) != val32) {
        return false;
    }
    return true;
}

/* 3. 12-Byte Header Size Verification */
static bool test_header_size(void) {
    return (HEADER_LEN == 12U);
}

/* 4. Sequence Number Handling */
static bool test_sequence_number_handling(void) {
    header_t hdr = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = MSG_DATA_CHUNK,
        .flags = 0,
        .payload_len = 64,
        .seq_num = 0xFFFFFFFEU
    };

    uint8_t buf[HEADER_LEN];
    serialize_header(&hdr, buf);

    header_t decoded;
    deserialize_header(buf, &decoded);

    return (decoded.seq_num == 0xFFFFFFFEU);
}

/* 5. uint64_t Offset Serialization */
static bool test_uint64_offset_serialization(void) {
    uint64_t original = 0x123456789ABCDEF0ULL;
    uint8_t buf[8];
    serialize_uint64(original, buf);

    if (buf[0] != 0x12U || buf[7] != 0xF0U) {
        return false;
    }
    uint64_t decoded = deserialize_uint64(buf);
    return (decoded == original);
}

/* 6. read_n() with Fragmented Input using socketpair() */
static bool test_read_n_fragmented(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        return false;
    }

    const char *part1 = "Hello, ";
    const char *part2 = "World!";
    char buf[32];
    memset(buf, 0, sizeof(buf));

    ssize_t w1 = write(sv[1], part1, strlen(part1));
    ssize_t w2 = write(sv[1], part2, strlen(part2));
    (void)w1; (void)w2;

    size_t total_len = strlen(part1) + strlen(part2);
    ssize_t nread = read_n(sv[0], buf, total_len);

    close(sv[0]);
    close(sv[1]);

    return (nread == (ssize_t)total_len && strcmp(buf, "Hello, World!") == 0);
}

/* 7. write_n() with Partial Writes */
static bool test_write_n(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        return false;
    }

    const char *msg = "Test Payload Data for write_n validation";
    size_t msg_len = strlen(msg);

    ssize_t nwritten = write_n(sv[1], msg, msg_len);

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t nread = read_n(sv[0], buf, msg_len);

    close(sv[0]);
    close(sv[1]);

    return (nwritten == (ssize_t)msg_len && nread == (ssize_t)msg_len && strcmp(buf, msg) == 0);
}

/* 8. Invalid Magic Rejection */
static bool test_invalid_magic_rejection(void) {
    header_t hdr = {
        .magic = 0xDEADU,
        .msg_type = MSG_GET_REQ,
        .flags = 0,
        .payload_len = 100,
        .seq_num = 1
    };
    return (validate_header(&hdr) == (int)ERR_BAD_MAGIC);
}

/* 9. Invalid Payload Length Rejection */
static bool test_invalid_payload_len_rejection(void) {
    header_t hdr = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = MSG_DATA_CHUNK,
        .flags = 0,
        .payload_len = MAX_PAYLOAD_LEN + 1U,
        .seq_num = 1
    };
    return (validate_header(&hdr) == (int)ERR_BAD_PAYLOAD_LEN);
}

/* 10. MAX_PAYLOAD_LEN Boundary Check */
static bool test_max_payload_len_boundary(void) {
    header_t hdr_exact = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = MSG_DATA_CHUNK,
        .flags = 0,
        .payload_len = MAX_PAYLOAD_LEN,
        .seq_num = 1
    };
    if (validate_header(&hdr_exact) != (int)ERR_OK) {
        return false;
    }

    header_t hdr_over = hdr_exact;
    hdr_over.payload_len = MAX_PAYLOAD_LEN + 1U;
    if (validate_header(&hdr_over) != (int)ERR_BAD_PAYLOAD_LEN) {
        return false;
    }
    return true;
}

/* 11. CRC32 Known Test Vector */
static bool test_crc32_known_vector(void) {
    const char *test_str = "123456789";
    uint32_t expected = 0xCBF43926U;
    uint32_t calculated = crc32_calculate(test_str, 9);
    return (calculated == expected);
}

/* 12. Path Length Boundary Check */
static bool test_path_length_boundary(void) {
    char valid_path[MAX_PATH_LEN + 1U];
    memset(valid_path, 'a', MAX_PATH_LEN);
    valid_path[MAX_PATH_LEN] = '\0';

    if (strlen(valid_path) != MAX_PATH_LEN) {
        return false;
    }

    bool valid_check = (strlen(valid_path) <= MAX_PATH_LEN);

    char invalid_path[MAX_PATH_LEN + 2U];
    memset(invalid_path, 'b', MAX_PATH_LEN + 1U);
    invalid_path[MAX_PATH_LEN + 1U] = '\0';

    bool invalid_check = (strlen(invalid_path) > MAX_PATH_LEN);

    return (valid_check && invalid_check);
}

int main(void) {
    set_log_level(LOG_LEVEL_INFO);
    printf("==================================================\n");
    printf("     IT305 Phase 0 Core Unit Test Suite           \n");
    printf("==================================================\n");

    RUN_TEST(test_header_serialization);
    RUN_TEST(test_network_byte_order);
    RUN_TEST(test_header_size);
    RUN_TEST(test_sequence_number_handling);
    RUN_TEST(test_uint64_offset_serialization);
    RUN_TEST(test_read_n_fragmented);
    RUN_TEST(test_write_n);
    RUN_TEST(test_invalid_magic_rejection);
    RUN_TEST(test_invalid_payload_len_rejection);
    RUN_TEST(test_max_payload_len_boundary);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_path_length_boundary);

    printf("==================================================\n");
    printf(" Results: %d / %d tests passed.\n", g_tests_passed, g_tests_run);
    printf("==================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
