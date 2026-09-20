#pragma once
/* ARM newlib has no <endian.h>; provide the macros org.cpp needs. */
#include <stdint.h>

#ifndef __BYTE_ORDER
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER    __LITTLE_ENDIAN
#endif

#ifndef htole16
static inline uint16_t htole16(uint16_t x) { return x; }
static inline uint32_t htole32(uint32_t x) { return x; }
static inline uint16_t le16toh(uint16_t x) { return x; }
static inline uint32_t le32toh(uint32_t x) { return x; }
static inline uint16_t htobe16(uint16_t x)
{
    return (uint16_t)((x << 8) | (x >> 8));
}
static inline uint32_t htobe32(uint32_t x)
{
    return ((x & 0xff) << 24) | ((x & 0xff00) << 8) |
           ((x & 0xff0000) >> 8) | ((x >> 24) & 0xff);
}
static inline uint16_t be16toh(uint16_t x) { return htobe16(x); }
static inline uint32_t be32toh(uint32_t x) { return htobe32(x); }
#endif
