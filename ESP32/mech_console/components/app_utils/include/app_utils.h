/*
 * @file    app_utils.h
 * @author  maxin
 * @date    2026-10-31
 * @brief   Shared little-endian byte read/write helpers.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

/** @brief  Write a 16-bit value to @p dst in little-endian order */
static inline void app_write_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

/** @brief  Write a 32-bit value to @p dst in little-endian order */
static inline void app_write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

/** @brief  Write a 64-bit value to @p dst in little-endian order */
static inline void app_write_u64_le(uint8_t *dst, uint64_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
    dst[2] = (uint8_t)((value >> 16) & 0xFF);
    dst[3] = (uint8_t)((value >> 24) & 0xFF);
    dst[4] = (uint8_t)((value >> 32) & 0xFF);
    dst[5] = (uint8_t)((value >> 40) & 0xFF);
    dst[6] = (uint8_t)((value >> 48) & 0xFF);
    dst[7] = (uint8_t)((value >> 56) & 0xFF);
}

/** @brief  Read a little-endian 16-bit value from @p src */
static inline uint16_t app_read_u16_le(const uint8_t *src)
{
    return (uint16_t)(src[0] | (src[1] << 8));
}

/** @brief  Read a little-endian 32-bit value from @p src */
static inline uint32_t app_read_u32_le(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

/** @brief  Read a little-endian 64-bit value from @p src */
static inline uint64_t app_read_u64_le(const uint8_t *src)
{
    return (uint64_t)src[0] | ((uint64_t)src[1] << 8) | ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) | ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
}
