#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#define PROTOCOL_MAGIC        0x4954U /* 'I', 'T' (0x49, 0x54) */
#define HEADER_LEN            12U
#define MAX_PAYLOAD_LEN       65536U
#define MAX_PATH_LEN          4096U
#define SESSION_ID_LEN        32U     /* 32 hex characters */
#define SESSION_ID_BUF_SIZE   33U     /* 32 chars + null terminator */
#define TOPIC_NAME_MAX_LEN    64U
#define TOPIC_NAME_BUF_SIZE   65U

/* Bit Flags */
#define FLAG_RESUME           0x01U
#define FLAG_EOF_FILE         0x02U
#define FLAG_FINAL_DONE       0x04U
#define FLAG_ERR              0x08U

#define VALID_FLAGS_MASK      (FLAG_RESUME | FLAG_EOF_FILE | FLAG_FINAL_DONE | FLAG_ERR)

/* Error Codes */
#define ERR_OK                0x0000U
#define ERR_PATH_TOO_LONG     0x0400U
#define ERR_BAD_MAGIC         0x0401U
#define ERR_BAD_PAYLOAD_LEN   0x0402U
#define ERR_BAD_MSG_TYPE      0x0403U
#define ERR_TOPIC_NOT_FOUND   0x0404U
#define ERR_BAD_FLAGS         0x0405U
#define ERR_INVALID_SESSION   0x0409U
#define ERR_INTERNAL_SERVER   0x0500U
#define ERR_SERVER_FULL       0x0503U

/* Message Types */
typedef enum {
    MSG_GET_REQ         = 0x01,
    MSG_MANIFEST_START  = 0x02,
    MSG_FILE_HEADER     = 0x03,
    MSG_DATA_CHUNK      = 0x04,
    MSG_ACK             = 0x05,
    MSG_TRANSFER_DONE   = 0x06,
    MSG_ERROR           = 0x07,
    MSG_RANGE_REQ       = 0x08,
    MSG_MANIFEST_ENTRY  = 0x09,
    MSG_MANIFEST_END    = 0x0A
} msg_type_t;

/*
 * 12-byte fixed application header layout:
 * [Magic (2B) | MsgType (1B) | Flags (1B) | PayloadLen (4B) | SeqNum (4B)]
 */
typedef struct {
    uint16_t magic;
    uint8_t  msg_type;
    uint8_t  flags;
    uint32_t payload_len;
    uint32_t seq_num;
} header_t;

/* Header Serialization & Deserialization */
void serialize_header(const header_t *hdr, uint8_t *buf);
void deserialize_header(const uint8_t *buf, header_t *hdr);

/* Scalar Field Serialization & Deserialization */
void serialize_uint16(uint16_t val, uint8_t *buf);
uint16_t deserialize_uint16(const uint8_t *buf);

void serialize_uint32(uint32_t val, uint8_t *buf);
uint32_t deserialize_uint32(const uint8_t *buf);

void serialize_uint64(uint64_t val, uint8_t *buf);
uint64_t deserialize_uint64(const uint8_t *buf);

/* Header Validation */
int validate_header(const header_t *hdr);

/* TCP I/O Helper Functions */
ssize_t read_n(int fd, void *buf, size_t n);
ssize_t write_n(int fd, const void *buf, size_t n);

#endif /* PROTOCOL_H */
