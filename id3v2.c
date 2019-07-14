#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>
#include "id3v2.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }

static const char *PCNT_ID = "PCNT";

DB_id3v2_frame_t *id3v2_frame_pcnt_create() {
#ifdef DEBUG
    trace("Creating a new PCNT frame.\n")
#endif
    const size_t data_size = sizeof(uint32_t);
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

uint8_t id3v2_frame_pcnt_inc(DB_id3v2_frame_t *frame) {
#ifdef DEBUG
    trace("Incrementing PCNT frame count.\n")
#endif
    if (sizeof(uint32_t) == frame->size) {
        uint32_t count = ntohl(*(uint32_t *) frame->data);
        if (UINT32_MAX == count) { return 1; }
        *((uint32_t *) frame->data) = htonl(count + 1);
#ifdef DEBUG
        trace("Read count of %d, wrote count of %d.\n", count, count + 1)
        uint32_t new_count = ntohl(*(uint32_t *) frame->data);
        trace("Actual new count is %d.\n", new_count)
#endif
    } else {
        return 1;
    }

    return 0;
}

/**
 * Set the play count value of an existing PCNT frame.
 *
 * Should only be called directly for debugging purposes.
 *
 * @param frame  A pointer to the PCNT frame.
 * @param count  The play count to set.
 * @return  Return non-zero if an error occurred, zero otherwise.
 */
static uint8_t id3v2_frame_pcnt_set(DB_id3v2_frame_t *frame, uint32_t count) {
#ifdef DEBUG
    trace("Setting PCNT frame count to %d.\n", count)
#endif
    if (sizeof(uint32_t) == frame->size) {
        *((uint32_t *) frame->data) = htonl(count);
#ifdef DEBUG
        trace("Wrote count of %d.\n", count)
        uint32_t new_count = ntohl(*(uint32_t *) frame->data);
        trace("Actual new count is %d.\n", new_count)
#endif
        return 0;

    } else {
        return 1;
    }
}

uint8_t id3v2_frame_pcnt_reset(DB_id3v2_frame_t *frame) {
    return id3v2_frame_pcnt_set(frame, 0);
}

void id3v2_tag_frame_add(DB_id3v2_tag_t *tag, DB_id3v2_frame_t *frame) {
    DB_id3v2_frame_t *tail = NULL;
    for (tail = tag->frames; tail && tail->next; tail = tail->next);

    if (tail) {
        tail->next = frame;
    } else {
        tag->frames = frame;
    }
}

DB_id3v2_frame_t *id3v2_tag_frame_get_pcnt(DB_id3v2_tag_t *tag) {

    DB_id3v2_frame_t *current = tag->frames;

    while (current && strcmp(PCNT_ID, current->id)) {
        current = current->next;
    }

#ifdef DEBUG
    if (!current) { trace("PCNT frame not found.\n") }
#endif
    return current;
}

DB_id3v2_frame_t *id3v2_tag_frame_rem_pcnt(DB_id3v2_tag_t *tag) {

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
