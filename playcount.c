#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>

#include "id3v2.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define UNUSED(x) { (void) x; }

static DB_functions_t *deadbeef;

static const char *FILE_TYPE_TAG = ":FILETYPE";
static const char *LOCATION_TAG = ":URI";
static const char *TAG_TYPE_TAG = ":TAGS";

static const char *FILE_TYPE_MP3 = "MP3";
static const char *TAG_TYPE_ID3V2 = "ID3v2";

/**
 * Get track metadata required for play count updates.
 *
 * We're assuming that the given track won't be null, and that it will have
 * metadata. We leave it up to the caller to check returned values.
 *
 * @param track  A pointer to the track to get metadata information from.
 * @param file_type  A pointer to the file type string.
 * @param location  A pointer to the file location string.
 * @param tag_type  A pointer to the tag type string.
 */
static void pl_get_meta_pcnt(DB_playItem_t *track, const char **file_type,
                             const char **location, const char **tag_type) {

    deadbeef->pl_lock();
    DB_metaInfo_t *track_meta = deadbeef->pl_get_metadata_head(track);

    while (track_meta) {
        const char *key = track_meta->key;
        const char *val = track_meta->value;

        if (!strcmp(FILE_TYPE_TAG, key)) { *file_type = val; }
        else if (!strcmp(LOCATION_TAG, key)) { *location = val; }
        else if (!strcmp(TAG_TYPE_TAG, key)) { *tag_type = val; }

        if (*file_type && *location && *tag_type) { break; }

        track_meta = track_meta->next;
#ifdef DEBUG
        trace("Found metadata '%s': '%s'\n", key, val)
#endif
    }
    deadbeef->pl_unlock();
}

static int increment_track_playcount(DB_playItem_t *track) {
    // Get required track metadata.
    const char *track_file_type = NULL;
    const char *track_location = NULL;
    const char *track_tag_type = NULL;

    pl_get_meta_pcnt(track, &track_file_type, &track_location, &track_tag_type);

    // Note: API < 1.5 returns 0 for vfs.
    // TODO: Bug? local files start with '/', but that's classified as 'remote'.
    if (strncmp("/", track_location, 1) || !deadbeef->is_local_file(track_location)) {
        // Unable to update play count for remote audio, no file access.
        return 0;
    }

    // MP3 files can contain ID3v2 or APEv2 tags.
    if (!strcmp(FILE_TYPE_MP3, track_file_type)) {

        // Handle ID3v2 tags.
        if (strstr(track_tag_type, TAG_TYPE_ID3V2)) {

            // Update the frame if it exists, otherwise create and set it.
            DB_id3v2_tag_t id3v2 = {0};
            DB_FILE *track_file = deadbeef->fopen(track_location);
            deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);

            DB_id3v2_frame_t *pcnt = id3v2_tag_frame_get_pcnt(&id3v2);

            if (!pcnt) {
                trace("Didn't find PCNT frame, creating & incrementing count.\n")
                pcnt = id3v2_frame_pcnt_create();
                id3v2_tag_frame_add(&id3v2, pcnt);
            }

            trace("Incrementing PCNT frame count.\n")
            id3v2_frame_pcnt_inc(pcnt);

            trace("Writing file.\n")
            FILE *actual_file = fopen(track_location, "r+");
            fseek(actual_file, 0, SEEK_SET);
            deadbeef->junk_id3v2_write(actual_file, &id3v2);
            fclose(actual_file);

            trace("Freeing resources.\n")
            free(id3v2_tag_frame_rem_pcnt(&id3v2));
            deadbeef->junk_id3v2_free(&id3v2);
            deadbeef->fclose(track_file);
        }
    }

    return 0;
}

static int reset_track_playcount(DB_playItem_t *track) {
    // Get required track metadata.
    const char *track_file_type = NULL;
    const char *track_location = NULL;
    const char *track_metadata_type = NULL;

    pl_get_meta_pcnt(track, &track_file_type, &track_location, &track_metadata_type);

    // Note: API < 1.5 returns 0 for vfs.
    // TODO: Bug? local files start with '/', but that's classified as 'remote'.
    if (strncmp("/", track_location, 1) || !deadbeef->is_local_file(track_location)) {
        // Unable to update play count for remote audio, no file access.
        return 0;
    }

    // MP3 files can contain ID3v2 or APEv2 tags.
    if (!strcmp(FILE_TYPE_MP3, track_file_type)) {

        // Handle ID3v2 tags.
        if (strstr(track_metadata_type, TAG_TYPE_ID3V2)) {

            // If the frame exists then reset its count, but don't create it.
            DB_id3v2_tag_t id3v2 = {0};
            DB_FILE *track_file = deadbeef->fopen(track_location);
            deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);

            DB_id3v2_frame_t *pcnt = id3v2_tag_frame_get_pcnt(&id3v2);

            if (pcnt) {
                trace("Found PCNT frame, resetting count.\n")
                id3v2_frame_pcnt_reset(pcnt);

                trace("Writing file.\n")
                FILE *actual_file = fopen(track_location, "r+");
                fseek(actual_file, 0, SEEK_SET);
                deadbeef->junk_id3v2_write(actual_file, &id3v2);
                fclose(actual_file);
            }

            trace("Freeing resources.\n")
            deadbeef->junk_id3v2_free(&id3v2);
            deadbeef->fclose(track_file);
        }
    }

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
    return reset_track_playcount((DB_playItem_t *) userdata);
}

static int increment_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    UNUSED(action)
    return increment_track_playcount((DB_playItem_t *) userdata);
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

static DB_plugin_action_t *get_actions(DB_playItem_t *it) {
    UNUSED(it)
    return &increment_playcount_action;
}

#else
static DB_plugin_action_t* get_actions(DB_playItem_t *it) {
    UNUSED(it)
    return &reset_playcount_action;
}
#endif

static int handle_event(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    UNUSED(p1)
    UNUSED(p2)

    if (DB_EV_SONGFINISHED == id) {
        // Increment the play count after a song has finished playing (simple).
        ddb_event_track_t *event_track = (ddb_event_track_t *) ctx;
        return increment_track_playcount(event_track->track);
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
