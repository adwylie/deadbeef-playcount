#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>

#include "id3v2.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define UNUSED(x) { (void) x; }

static DB_functions_t *deadbeef;

static const char *LOCATION_TAG = ":URI";
static const char *TAG_TYPE_TAG = ":TAGS";

static const char *TAG_TYPE_ID3V2_3 = "ID3v2.3";
static const char *TAG_TYPE_ID3V2_4 = "ID3v2.4";

/**
 * Return whether a track is supported by the plugin (wrt/ tags).
 *
 * Only ID3v2 (2.3, 2.4) tags are supported.
 *
 * - ID3v1 doesn't have a play count frame/property.
 * - ID3v2.2 is obsolete, won't support.
 * - APEv1 & APEv2 don't have play count frames/properties.
 *
 * @param track  A pointer to the track to check support for.
 * @return  A positive integer if the track is supported, zero otherwise.
 */
static uint8_t is_track_tag_supported(DB_playItem_t *track) {

    if (track) {
        deadbeef->pl_lock();
        const char *track_location = deadbeef->pl_find_meta(track, LOCATION_TAG);
        const char *track_tag_type = deadbeef->pl_find_meta(track, TAG_TYPE_TAG);
        deadbeef->pl_unlock();

        // Note: API >= 1.5 returns 1 for vfs.
        const int is_local = deadbeef->is_local_file(track_location);
        const char *id3v2_3 = strstr(track_tag_type, TAG_TYPE_ID3V2_3);
        const char *id3v2_4 = strstr(track_tag_type, TAG_TYPE_ID3V2_4);

        if (is_local && (id3v2_3 || id3v2_4)) { return 1; }
    }

    return 0;
}

/**
 * Read the play count from the track's tag.
 *
 * @param track  A pointer to the track.
 * @return  The currently set play count value.
 */
static uintmax_t get_track_tag_playcount(DB_playItem_t *track) {

    deadbeef->pl_lock();
    const char *track_location = deadbeef->pl_find_meta(track, LOCATION_TAG);
    deadbeef->pl_unlock();

    // If the frame exists read its count.
    DB_id3v2_tag_t id3v2 = {0};
    DB_FILE *track_file = deadbeef->fopen(track_location);
    deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);

    DB_id3v2_frame_t *pcnt = id3v2_tag_get_pcnt_frame(&id3v2);
    if (pcnt) {
        return id3v2_pcnt_frame_get_count(pcnt);
    }

    deadbeef->junk_id3v2_free(&id3v2);
    deadbeef->fclose(track_file);
    return 0;
}

static uint8_t increment_track_tag_playcount(DB_playItem_t *track) {

    deadbeef->pl_lock();
    const char *track_location = deadbeef->pl_find_meta(track, LOCATION_TAG);
    deadbeef->pl_unlock();

    // Update the frame if it exists, otherwise create and set it.
    DB_id3v2_tag_t id3v2 = {0};
    DB_FILE *track_file = deadbeef->fopen(track_location);
    deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);

    DB_id3v2_frame_t *pcnt = id3v2_tag_get_pcnt_frame(&id3v2);
    uint8_t created = 0;

    if (!pcnt) {
        pcnt = id3v2_create_pcnt_frame();
        created = 1;
        id3v2_tag_add_frame(&id3v2, pcnt);
    }

    DB_id3v2_frame_t *updated = id3v2_pcnt_frame_inc_count(pcnt);
    if (updated != pcnt) {
        // A new frame was created on count increment.
        // Remove the old one, add the new one.
        // Should only be one PCNT frame, so: removed == pcnt.
        DB_id3v2_frame_t *removed = id3v2_tag_rem_pcnt_frame(&id3v2);
        if (created) { free(removed); }

        created = 1;
        id3v2_tag_add_frame(&id3v2, updated);
    }

    // Save the changes.
    FILE *actual_file = fopen(track_location, "r+");
    fseek(actual_file, 0, SEEK_SET);
    deadbeef->junk_id3v2_write(actual_file, &id3v2);
    fclose(actual_file);

    // Clean up resources.
    if (created) { free(id3v2_tag_rem_pcnt_frame(&id3v2)); }
    deadbeef->junk_id3v2_free(&id3v2);
    deadbeef->fclose(track_file);

    return 0;
}

/**
 * Write the given play count to the track's tag.
 *
 * @param track  A pointer to the track.
 * @param count  The play count to set.
 * @return  A positive integer if an error occurred, zero otherwise.
 */
static uint8_t set_track_tag_playcount(DB_playItem_t *track, uintmax_t count) {

    deadbeef->pl_lock();
    const char *track_location = deadbeef->pl_find_meta(track, LOCATION_TAG);
    deadbeef->pl_unlock();

    // If the frame exists reset its count, but don't create it.
    DB_id3v2_tag_t id3v2 = {0};
    DB_FILE *track_file = deadbeef->fopen(track_location);
    deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);

    DB_id3v2_frame_t *pcnt = id3v2_tag_get_pcnt_frame(&id3v2);
    uint8_t created = 0;

    if (pcnt) {
        DB_id3v2_frame_t *updated = id3v2_pcnt_frame_set_count(pcnt, count);

        if (updated != pcnt) {
            // A new frame was created on count reset.
            // Remove the old one, add the new one.
            // Should only be one PCNT frame, so: removed == pcnt.
            id3v2_tag_rem_pcnt_frame(&id3v2);

            created = 1;
            id3v2_tag_add_frame(&id3v2, updated);
        }

        // Save the changes.
        FILE *actual_file = fopen(track_location, "r+");
        fseek(actual_file, 0, SEEK_SET);
        deadbeef->junk_id3v2_write(actual_file, &id3v2);
        fclose(actual_file);
    }

    // Clean up resources.
    if (created) { free(id3v2_tag_rem_pcnt_frame(&id3v2)); }
    deadbeef->junk_id3v2_free(&id3v2);
    deadbeef->fclose(track_file);

    return 0;
}

static int start() {
    // Note: Plugin will be unloaded if start returns -1.
    return 0;
}

static int stop() {
    return 0;
}

static int reset_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    // Note: When called from context menu function seems to be called once per
    //       track. The 'void *userdata' is a pointer to the track.
    UNUSED(action)
    return set_track_tag_playcount((DB_playItem_t *) userdata, 0);
}

static int increment_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    UNUSED(action)
    return increment_track_tag_playcount((DB_playItem_t *) userdata);
}

// Add action(s) to the song context menu.
static DB_plugin_action_t reset_playcount_action = {
        .title = "Reset Playcount",
        .name = "reset_playcount",
        .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_MULTIPLE_TRACKS,
        .callback = reset_playcount_callback,
        .next = NULL
};

#ifdef DEBUG
static DB_plugin_action_t increment_playcount_action = {
        .title = "Increment Playcount",
        .name = "increment_playcount",
        .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_MULTIPLE_TRACKS,
        .callback = increment_playcount_callback,
        .next = &reset_playcount_action
};

static int get_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    UNUSED(action)

    deadbeef->pl_lock();
    const char *title = deadbeef->pl_find_meta((DB_playItem_t *) userdata, "title");
    deadbeef->pl_unlock();

    trace("%s: %" PRIuMAX "\n",
            title, get_track_tag_playcount((DB_playItem_t *) userdata))

    return 0;
}

static DB_plugin_action_t get_playcount_action = {
        .title = "Get Playcount",
        .name = "get_playcount",
        .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_MULTIPLE_TRACKS,
        .callback = get_playcount_callback,
        .next = &increment_playcount_action
};
#endif

static DB_plugin_action_t *get_actions(DB_playItem_t *it) {

    if (is_track_tag_supported(it)) {
#ifdef DEBUG
        return &get_playcount_action;
#else
        return &reset_playcount_action;
#endif
    }

    return NULL;
}

static int handle_event(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    UNUSED(p1)
    UNUSED(p2)

    // TODO: DB_EV_SONGFINISHED called when 'stop' button is clicked.
    if (DB_EV_SONGFINISHED == id) {
        // Increment the play count after a song has finished playing (simple).
        ddb_event_track_t *event_track = (ddb_event_track_t *) ctx;
        return increment_track_tag_playcount(event_track->track);
    }

    return 0;
}

static DB_misc_t plugin = {
    .plugin = {
        .type = DB_PLUGIN_MISC,
        .api_vmajor = 1,
        .api_vminor = 0,
        .version_major = PROJECT_VERSION_MAJOR,
        .version_minor = PROJECT_VERSION_MINOR,

        .name = "playcount",
        .descr = "keep track of song play counts",
        .copyright =
            "BSD 3-Clause License\n\n"
            "Copyright (c) 2019, Andrew Wylie\n"
            "All rights reserved.\n\n"
            "Redistribution and use in source and binary forms, with or without\n"
            "modification, are permitted provided that the following conditions are met:\n\n"
            "1. Redistributions of source code must retain the above copyright notice, this\n"
            "   list of conditions and the following disclaimer.\n\n"
            "2. Redistributions in binary form must reproduce the above copyright notice,\n"
            "   this list of conditions and the following disclaimer in the documentation\n"
            "   and/or other materials provided with the distribution.\n\n"
            "3. Neither the name of the copyright holder nor the names of its\n"
            "   contributors may be used to endorse or promote products derived from\n"
            "   this software without specific prior written permission.\n\n"
            "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"
            "AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
            "IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"
            "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n"
            "FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
            "DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n"
            "SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n"
            "CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n"
            "OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
            "OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.",
        .website = "https://github.com/adwylie/deadbeef-playcount",

        .start = start,
        .stop = stop,
        .connect = NULL,
        .disconnect = NULL,
        .exec_cmdline = NULL,
        .get_actions = get_actions,
        .message = handle_event,
        .configdialog = NULL
    }
};

extern DB_plugin_t *playcount_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
