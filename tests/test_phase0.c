#include "protocol.h"
#include "checksum.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    printf("Running %-38s ... ", #fn); \
    if (fn()) { \
        printf("PASS\n"); \
        g_tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while (0)

static void sleep_ms(int ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
}

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

/* 6. read_n() with Fragmented Input using Socketpair and Writer Thread */
typedef struct {
    int fd;
} read_frag_args_t;

static void *read_frag_writer(void *arg) {
    read_frag_args_t *args = (read_frag_args_t *)arg;
    int fd = args->fd;
    const char *p1 = "Part1_";
    const char *p2 = "Part2_";
    const char *p3 = "Part3_End";

    /* Write Part 1 */
    if (write(fd, p1, strlen(p1)) <= 0) return NULL;
    sleep_ms(5); /* 5ms delay ensures reader's recv() consumes p1 and blocks for p2 */

    /* Write Part 2 */
    if (write(fd, p2, strlen(p2)) <= 0) return NULL;
    sleep_ms(5); /* 5ms delay ensures reader's recv() consumes p2 and blocks for p3 */

    /* Write Part 3 */
    if (write(fd, p3, strlen(p3)) <= 0) return NULL;

    return NULL;
}

static bool test_read_n_fragmented(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        return false;
    }

    read_frag_args_t args = { .fd = sv[1] };
    pthread_t thread;
    if (pthread_create(&thread, NULL, read_frag_writer, &args) != 0) {
        close(sv[0]);
        close(sv[1]);
        return false;
    }

    const char *expected = "Part1_Part2_Part3_End";
    size_t total_len = strlen(expected);
    char buf[64];
    memset(buf, 0, sizeof(buf));

    /* Main thread calls read_n() requesting full length while writer sends fragments with micro-delays */
    ssize_t nread = read_n(sv[0], buf, total_len);

    pthread_join(thread, NULL);
    close(sv[0]);
    close(sv[1]);

    return (nread == (ssize_t)total_len && strcmp(buf, expected) == 0);
}

/* 7. write_n() under Controlled Partial Send & Socket Backpressure */
typedef struct {
    int fd;
    size_t payload_size;
    const uint8_t *tx_buf;
    ssize_t nwritten;
} write_n_args_t;

static void *write_n_sender_thread(void *arg) {
    write_n_args_t *wargs = (write_n_args_t *)arg;
    wargs->nwritten = write_n(wargs->fd, wargs->tx_buf, wargs->payload_size);
    return NULL;
}

static bool test_write_n(void) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
        return false;
    }

    /* Configure small socket send & receive buffer limits to force backpressure */
    int sndbuf = 4096;
    int rcvbuf = 4096;
    setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    /* 128 KiB payload (32x larger than 4 KiB socket buffer) */
    size_t payload_size = 131072;
    uint8_t *tx_buf = malloc(payload_size);
    uint8_t *rx_buf = malloc(payload_size);
    if (!tx_buf || !rx_buf) {
        free(tx_buf);
        free(rx_buf);
        close(sv[0]);
        close(sv[1]);
        return false;
    }

    for (size_t i = 0; i < payload_size; i++) {
        tx_buf[i] = (uint8_t)(i & 0xFFU);
    }

    write_n_args_t wargs = {
        .fd = sv[1],
        .payload_size = payload_size,
        .tx_buf = tx_buf,
        .nwritten = -1
    };

    pthread_t thread;
    if (pthread_create(&thread, NULL, write_n_sender_thread, &wargs) != 0) {
        free(tx_buf);
        free(rx_buf);
        close(sv[0]);
        close(sv[1]);
        return false;
    }

    /* Main thread consumes data in small 2 KiB chunks with micro-delays to generate continuous backpressure */
    size_t total_received = 0;
    while (total_received < payload_size) {
        size_t chunk_to_read = 2048;
        if (payload_size - total_received < chunk_to_read) {
            chunk_to_read = payload_size - total_received;
        }

        ssize_t nread = read_n(sv[0], rx_buf + total_received, chunk_to_read);
        if (nread <= 0) {
            break;
        }
        total_received += (size_t)nread;
        sleep_ms(1); /* 1ms micro-delay maintains socket backpressure on sender */
    }

    pthread_join(thread, NULL);

    bool ok = (wargs.nwritten == (ssize_t)payload_size &&
               total_received == payload_size &&
               memcmp(tx_buf, rx_buf, payload_size) == 0);

    free(tx_buf);
    free(rx_buf);
    close(sv[0]);
    close(sv[1]);
    return ok;
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
    return (validate_header(&hdr_over) == (int)ERR_BAD_PAYLOAD_LEN);
}

/* 11. Invalid Message Type Rejection */
static bool test_invalid_msg_type_rejection(void) {
    header_t hdr = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = 0xFFU,
        .flags = 0,
        .payload_len = 64,
        .seq_num = 1
    };
    return (validate_header(&hdr) == (int)ERR_BAD_MSG_TYPE);
}

/* 12. Reserved Flag Bits Rejection */
static bool test_reserved_flag_rejection(void) {
    header_t hdr = {
        .magic = PROTOCOL_MAGIC,
        .msg_type = MSG_GET_REQ,
        .flags = 0x80U, /* Reserved flag bit */
        .payload_len = 64,
        .seq_num = 1
    };
    return (validate_header(&hdr) == (int)ERR_BAD_FLAGS);
}

/* 13. CRC32 Known Test Vector */
static bool test_crc32_known_vector(void) {
    const char *test_str = "123456789";
    uint32_t expected = 0xCBF43926U;
    uint32_t calculated = crc32_calculate(test_str, 9);
    return (calculated == expected);
}

/* 14. Lock-Free Concurrent CRC32 Multithreaded Test */
typedef struct {
    int thread_id;
    bool success;
} crc_thread_arg_t;

static void *crc_worker(void *arg) {
    crc_thread_arg_t *targ = (crc_thread_arg_t *)arg;
    targ->success = true;

    const char *test_str = "123456789";
    uint32_t expected = 0xCBF43926U;

    for (int i = 0; i < 5000; i++) {
        uint32_t res = crc32_calculate(test_str, 9);
        if (res != expected) {
            targ->success = false;
            break;
        }
    }
    return NULL;
}

static bool test_crc32_concurrency(void) {
    #define NUM_CRC_THREADS 8
    pthread_t threads[NUM_CRC_THREADS];
    crc_thread_arg_t args[NUM_CRC_THREADS];

    for (int i = 0; i < NUM_CRC_THREADS; i++) {
        args[i].thread_id = i;
        args[i].success = false;
        if (pthread_create(&threads[i], NULL, crc_worker, &args[i]) != 0) {
            return false;
        }
    }

    bool overall_success = true;
    for (int i = 0; i < NUM_CRC_THREADS; i++) {
        pthread_join(threads[i], NULL);
        if (!args[i].success) {
            overall_success = false;
        }
    }

    return overall_success;
}

/* 15. Untrusted Relative Path Traversal Validation */
static bool test_path_traversal_protection(void) {
    /* REJECT cases */
    if (is_safe_relative_path(NULL)) return false;
    if (is_safe_relative_path("")) return false;
    if (is_safe_relative_path("/abs/path.txt")) return false;
    if (is_safe_relative_path(".")) return false;
    if (is_safe_relative_path("..")) return false;
    if (is_safe_relative_path("../file")) return false;
    if (is_safe_relative_path("../../file")) return false;
    if (is_safe_relative_path("a/../b")) return false;
    if (is_safe_relative_path("a/../../b")) return false;
    if (is_safe_relative_path("./file")) return false;
    if (is_safe_relative_path("a//b")) return false;

    /* ACCEPT cases */
    if (!is_safe_relative_path("file.txt")) return false;
    if (!is_safe_relative_path("animal.jpg")) return false;
    if (!is_safe_relative_path("subdir/file.txt")) return false;
    if (!is_safe_relative_path("abc..def")) return false;
    if (!is_safe_relative_path("a.b/c.txt")) return false;

    return true;
}

/* 16. Strict Unsigned Integer Parsing Test */
static bool test_strict_uint_parsing(void) {
    uint16_t val16;
    uint32_t val32;

    /* ACCEPT */
    if (!parse_uint16("0", &val16) || val16 != 0) return false;
    if (!parse_uint16("1", &val16) || val16 != 1) return false;
    if (!parse_uint16("65535", &val16) || val16 != 65535) return false;

    if (!parse_uint32("0", &val32) || val32 != 0) return false;
    if (!parse_uint32("1", &val32) || val32 != 1) return false;
    if (!parse_uint32("4294967295", &val32) || val32 != 4294967295U) return false;

    /* REJECT */
    if (parse_uint16("", &val16)) return false;
    if (parse_uint16(NULL, &val16)) return false;
    if (parse_uint16("-1", &val16)) return false;
    if (parse_uint16("+1", &val16)) return false;
    if (parse_uint16(" 1", &val16)) return false;
    if (parse_uint16("1 ", &val16)) return false;
    if (parse_uint16("\t1", &val16)) return false;
    if (parse_uint16("1\t", &val16)) return false;
    if (parse_uint16("12abc", &val16)) return false;
    if (parse_uint16("abc12", &val16)) return false;
    if (parse_uint16("65536", &val16)) return false;

    if (parse_uint32("-1", &val32)) return false;
    if (parse_uint32("+1", &val32)) return false;
    if (parse_uint32(" 1", &val32)) return false;
    if (parse_uint32("1 ", &val32)) return false;
    if (parse_uint32("4294967296", &val32)) return false;

    return true;
}

/* 17. Path Length Boundary Test */
static bool test_path_length_boundary(void) {
    char valid_path[MAX_PATH_LEN + 1U];
    memset(valid_path, 'a', MAX_PATH_LEN);
    valid_path[MAX_PATH_LEN] = '\0';

    if (!is_safe_relative_path(valid_path)) return false;

    char invalid_path[MAX_PATH_LEN + 2U];
    memset(invalid_path, 'b', MAX_PATH_LEN + 1U);
    invalid_path[MAX_PATH_LEN + 1U] = '\0';

    if (is_safe_relative_path(invalid_path)) return false;

    return true;
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
    RUN_TEST(test_invalid_msg_type_rejection);
    RUN_TEST(test_reserved_flag_rejection);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc32_concurrency);
    RUN_TEST(test_path_traversal_protection);
    RUN_TEST(test_strict_uint_parsing);
    RUN_TEST(test_path_length_boundary);

    printf("==================================================\n");
    printf(" Results: %d / %d tests passed.\n", g_tests_passed, g_tests_run);
    printf("==================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
