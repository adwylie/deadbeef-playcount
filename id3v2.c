#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>
#include "id3v2.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }

static const size_t DEFAULT_DATA_SIZE = sizeof(uint32_t);
static const char *PCNT_ID = "PCNT";

/**
 * Create/allocate a new PCNT frame object on the heap.
 *
 * @param data_size  The counter data size in bytes.
 * @return  A pointer to the created frame.
 */
static DB_id3v2_frame_t *id3v2_create_full_pcnt_frame(size_t data_size) {
#ifdef DEBUG
    trace("Creating a new PCNT frame.\n")
#endif
    DB_id3v2_frame_t *frame = malloc(sizeof(DB_id3v2_frame_t) + data_size);

    if (frame) {
        // Initialize the entire frame, as all flags should
        // be cleared and the counter should begin at zero.
        memset(frame, 0, sizeof(DB_id3v2_frame_t) + data_size);
        strcpy(frame->id, PCNT_ID);
        frame->size = data_size;
    }

    return frame;
}

DB_id3v2_frame_t *id3v2_create_pcnt_frame() {
    return id3v2_create_full_pcnt_frame(DEFAULT_DATA_SIZE);
}

DB_id3v2_frame_t *id3v2_pcnt_frame_inc_count(DB_id3v2_frame_t *frame) {
    // Data is stored in big endian (network byte order). We'll modify it in
    // place in memory instead of converting to host bye order and back.
    // Scan from the right-most bit to find the first unset bit.
    uint8_t position = 0;
    uint8_t mask = 1;
    uint8_t *window = frame->data + frame->size - 1;

    while (*window & mask) {
        mask <<= 1u;
        position++;

        // Reset the mask & window when we would start reading the next byte.
        if (!(position % (sizeof(*window) * CHAR_BIT))) {
            mask = 1;
            window -= 1;
        }

        // Determine if we've overrun the data (reading into frame->flags).
        // Reallocate adding an additional byte for play count value storage.
        // Then set the play count. Right-most bit in first byte should be set,
        // it's the 'new' byte that was just added.
        if (window < frame->data) {
            DB_id3v2_frame_t *f = id3v2_create_full_pcnt_frame(frame->size + 1);
            if (f) { (*((uint8_t *) f->data)) = 1; }
            return f;
        }
    }

    // Set the first unset bit.
    *window |= mask;

    // Clear all bits to the right.
    uint8_t clear_position = 0;
    mask = 1u;
    window = frame->data + frame->size - 1;

    while (clear_position < position) {
        *window &= ~mask;

        mask <<= 1u;
        clear_position++;

        // Reset the mask & window when we would start reading the next byte.
        if (!(clear_position % (sizeof(*window) * CHAR_BIT))) {
            mask = 1;
            window -= 1;
        }
    }

    return frame;
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

#ifdef DEBUG
    if (!current) { trace("PCNT frame not found.\n") }
#endif
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
