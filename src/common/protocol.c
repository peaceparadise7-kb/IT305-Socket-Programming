#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

void serialize_uint16(uint16_t val, uint8_t *buf) {
    buf[0] = (uint8_t)((val >> 8) & 0xFFU);
    buf[1] = (uint8_t)(val & 0xFFU);
}

uint16_t deserialize_uint16(const uint8_t *buf) {
    return (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
}

void serialize_uint32(uint32_t val, uint8_t *buf) {
    buf[0] = (uint8_t)((val >> 24) & 0xFFU);
    buf[1] = (uint8_t)((val >> 16) & 0xFFU);
    buf[2] = (uint8_t)((val >> 8) & 0xFFU);
    buf[3] = (uint8_t)(val & 0xFFU);
}

uint32_t deserialize_uint32(const uint8_t *buf) {
    return (((uint32_t)buf[0] << 24) |
            ((uint32_t)buf[1] << 16) |
            ((uint32_t)buf[2] << 8)  |
            ((uint32_t)buf[3]));
}

void serialize_uint64(uint64_t val, uint8_t *buf) {
    buf[0] = (uint8_t)((val >> 56) & 0xFFU);
    buf[1] = (uint8_t)((val >> 48) & 0xFFU);
    buf[2] = (uint8_t)((val >> 40) & 0xFFU);
    buf[3] = (uint8_t)((val >> 32) & 0xFFU);
    buf[4] = (uint8_t)((val >> 24) & 0xFFU);
    buf[5] = (uint8_t)((val >> 16) & 0xFFU);
    buf[6] = (uint8_t)((val >> 8) & 0xFFU);
    buf[7] = (uint8_t)(val & 0xFFU);
}

uint64_t deserialize_uint64(const uint8_t *buf) {
    return (((uint64_t)buf[0] << 56) |
            ((uint64_t)buf[1] << 48) |
            ((uint64_t)buf[2] << 40) |
            ((uint64_t)buf[3] << 32) |
            ((uint64_t)buf[4] << 24) |
            ((uint64_t)buf[5] << 16) |
            ((uint64_t)buf[6] << 8)  |
            ((uint64_t)buf[7]));
}

void serialize_header(const header_t *hdr, uint8_t *buf) {
    serialize_uint16(hdr->magic, buf);
    buf[2] = hdr->msg_type;
    buf[3] = hdr->flags;
    serialize_uint32(hdr->payload_len, buf + 4);
    serialize_uint32(hdr->seq_num, buf + 8);
}

void deserialize_header(const uint8_t *buf, header_t *hdr) {
    hdr->magic = deserialize_uint16(buf);
    hdr->msg_type = buf[2];
    hdr->flags = buf[3];
    hdr->payload_len = deserialize_uint32(buf + 4);
    hdr->seq_num = deserialize_uint32(buf + 8);
}

int validate_header(const header_t *hdr) {
    if (hdr->magic != PROTOCOL_MAGIC) {
        return (int)ERR_BAD_MAGIC;
    }
    if (hdr->payload_len > MAX_PAYLOAD_LEN) {
        return (int)ERR_BAD_PAYLOAD_LEN;
    }
    if ((hdr->flags & ~VALID_FLAGS_MASK) != 0U) {
        return (int)ERR_BAD_FLAGS;
    }
    switch (hdr->msg_type) {
        case MSG_GET_REQ:
        case MSG_MANIFEST_START:
        case MSG_FILE_HEADER:
        case MSG_DATA_CHUNK:
        case MSG_ACK:
        case MSG_TRANSFER_DONE:
        case MSG_ERROR:
        case MSG_RANGE_REQ:
        case MSG_MANIFEST_ENTRY:
        case MSG_MANIFEST_END:
            break;
        default:
            return (int)ERR_BAD_MSG_TYPE;
    }
    return (int)ERR_OK;
}

ssize_t read_n(int fd, void *buf, size_t n) {
    size_t total_read = 0;
    uint8_t *ptr = (uint8_t *)buf;

    while (total_read < n) {
        ssize_t nread = recv(fd, ptr + total_read, n - total_read, 0);
        if (nread > 0) {
            total_read += (size_t)nread;
        } else if (nread == 0) {
            return (ssize_t)total_read;
        } else {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
    }
    return (ssize_t)total_read;
}

ssize_t write_n(int fd, const void *buf, size_t n) {
    size_t total_written = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total_written < n) {
        ssize_t nwritten = send(fd, ptr + total_written, n - total_written, 0);
        if (nwritten > 0) {
            total_written += (size_t)nwritten;
        } else if (nwritten == 0) {
            return (ssize_t)total_written;
        } else {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
    }
    return (ssize_t)total_written;
}
