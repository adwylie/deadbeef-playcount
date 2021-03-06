/* Copyright (c) 2019, Andrew Wylie. All rights reserved.   */
/* Distributed under the terms of the 3-Clause BSD License. */
/* Full license text available in 'LICENSE' file.           */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>
#include "id3v2.h"

static const size_t DEFAULT_DATA_SIZE = sizeof(uint32_t);
static const char *PCNT_ID = "PCNT";

/**
 * Create/allocate a new PCNT frame object on the heap.
 *
 * @param data_size  The counter data size in bytes.
 * @return  A pointer to the created frame.
 */
static DB_id3v2_frame_t *id3v2_create_full_pcnt_frame(size_t data_size) {
    DB_id3v2_frame_t *frame = malloc(data_size + sizeof *frame);

    if (frame) {
        // All flags should be cleared, and the counter begins at zero.
        memset(frame, 0, data_size + sizeof *frame);
        strcpy(frame->id, PCNT_ID);
        frame->size = data_size;
    }

    return frame;
}

DB_id3v2_frame_t *id3v2_create_pcnt_frame() {
    return id3v2_create_full_pcnt_frame(DEFAULT_DATA_SIZE);
}

uintmax_t id3v2_pcnt_frame_get_count(DB_id3v2_frame_t *frame) {

    // Check if we can actually display the play count value.
    if (frame->size > sizeof(uintmax_t)) { return UINTMAX_MAX; }

    uintmax_t ret = 0;
    uint32_t byte_width = frame->size;

    // Working with memory so consider endianness of the host. Note that we
    // are also converting the value from network byte order (big endian).
    for (uint32_t i = 0; i < byte_width; i++) {
#if BYTE_ORDER == BIG_ENDIAN
        uint32_t j = i;
        uint32_t offset = sizeof(uintmax_t) - byte_width;
#elif BYTE_ORDER == LITTLE_ENDIAN
        uint32_t j = byte_width - i - 1;
        uint32_t offset = 0;
#endif
        *(((uint8_t *) (&ret)) + offset + i) = *(((uint8_t *) frame->data) + j);
    }

    return ret;
}

// Endianness refers to how data is stored in memory, however when operating
// on values in the processor's register they're represented in big endian.
DB_id3v2_frame_t *id3v2_pcnt_frame_set_count(
        DB_id3v2_frame_t *frame, uintmax_t count) {

    // Find the minimum number of bytes needed to store the count value.
    // Move from the LSB to MSB and identify where we see the last set bit.
    // Then round this bit position up to the nearest number of bytes.
    uint8_t bit_width = 0;
    uintmax_t mask = 1u;

    for (uint16_t i = 1; i <= sizeof(uintmax_t) * CHAR_BIT; i++) {
        if (count & mask) { bit_width = i; }
        mask <<= 1u;
    }

    uint8_t byte_width = bit_width / CHAR_BIT;
    if (bit_width % CHAR_BIT) { byte_width++; }
    if (byte_width < DEFAULT_DATA_SIZE) { byte_width = DEFAULT_DATA_SIZE; }

    // Compare widths of count and current frame->size.
    DB_id3v2_frame_t *ret = frame;

    if (frame->size != byte_width) {
        // If different create a new frame with a specific size.
        ret = id3v2_create_full_pcnt_frame(byte_width);
    }

    // Working with memory so consider endianness of the host. Note that we
    // are also converting the value to network byte order (big endian).
    for (uint8_t i = 0; i < byte_width; i++) {
#if BYTE_ORDER == BIG_ENDIAN
        uint8_t j = i;
        uint8_t offset = sizeof(uintmax_t) - byte_width;
#elif BYTE_ORDER == LITTLE_ENDIAN
        uint8_t j = byte_width - i - 1;
        uint8_t offset = 0;
#endif
        *(((uint8_t *) ret->data) + j) = *(((uint8_t *) (&count)) + offset + i);
    }

    return ret;
}

void id3v2_tag_add_frame(DB_id3v2_tag_t *tag, DB_id3v2_frame_t *frame) {
    DB_id3v2_frame_t *tail = NULL;
    for (tail = tag->frames; tail && tail->next; tail = tail->next);

    if (tail) {
        tail->next = frame;
    } else {
        tag->frames = frame;
    }
}

DB_id3v2_frame_t *id3v2_tag_get_pcnt_frame(DB_id3v2_tag_t *tag) {

    DB_id3v2_frame_t *current = tag->frames;

    while (current && strcmp(PCNT_ID, current->id)) {
        current = current->next;
    }

    return current;
}

DB_id3v2_frame_t *id3v2_tag_rem_pcnt_frame(DB_id3v2_tag_t *tag) {

    DB_id3v2_frame_t *current = tag->frames;
    if (!current) { return NULL; }

    if (!strcmp(PCNT_ID, current->id)) {
        tag->frames = current->next;
        current->next = NULL;
        return current;
    }

    DB_id3v2_frame_t *pcnt = current->next;

    while (pcnt && strcmp(PCNT_ID, pcnt->id)) {
        current = pcnt;
        pcnt = pcnt->next;
    }

    if (pcnt) {
        current->next = pcnt->next;
        pcnt->next = NULL;
    }

    return pcnt;
}
