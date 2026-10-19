/*
 * @file    frame_crc.h
 * @author  maxin
 * @date    2026-10-19
 * @brief   CRC32 calculation for frames.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "frame_common.h"

/**
 * @brief Calculate the CRC32 of a given data buffer.
 *
 * @param data Pointer to the input data.
 * @param length Length of the input data.
 * @param crc Pointer to the variable where the calculated CRC32 will be stored.
 * @return frame_codec_result_t Result of the CRC calculation operation.
 */
frame_codec_result_t frame_crc32(const uint8_t *data, size_t length, uint32_t *crc);
