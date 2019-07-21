#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <deadbeef.h>

#include "id3v2.h"

#define trace(...) { fprintf(stderr, __VA_ARGS__); }
#define UNUSED(x) { (void) x; }

static DB_functions_t *deadbeef;

static const char *PLAY_COUNT_META = "play_count";

static const char *LOCATION_TAG = ":URI";
static const char *TAG_TYPE_TAG = ":TAGS";

static const char *TAG_TYPE_ID3V2_3 = "ID3v2.3";
static const char *TAG_TYPE_ID3V2_4 = "ID3v2.4";

//
//  Metadata Operations.
//
/**
 * Return the meta play_count value.
 *
 * @param track  A pointer to the track.
 * @return  The play_count, or -1 if the value isn't set.
 */
static int get_track_meta_playcount(DB_playItem_t *track) {
    return deadbeef->pl_find_meta_int(track, PLAY_COUNT_META, -1);
}

static void set_track_meta_playcount(DB_playItem_t *track, int count) {
    deadbeef->pl_lock();
    deadbeef->pl_set_meta_int(track, PLAY_COUNT_META, count);
    deadbeef->pl_unlock();
}

//
//  Tag Operations
//
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

        if (!track_location || !track_tag_type) { return 0; }

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
 * If no PCNT frame exists a count of zero is returned.
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

    uintmax_t count = 0;
    DB_id3v2_frame_t *pcnt = id3v2_tag_get_pcnt_frame(&id3v2);
    if (pcnt) { count = id3v2_pcnt_frame_get_count(pcnt); }

    // Clean up resources.
    deadbeef->junk_id3v2_free(&id3v2);
    deadbeef->fclose(track_file);
    return count;
}

/**
 * Write the given play count to the track's tag.
 *
 * Creates the PCNT frame if one does not already exist.
 *
 * @param track  A pointer to the track.
 * @param count  The play count to set.
 * @return  A positive integer if an error occurred, zero otherwise.
 */
static uint8_t set_track_tag_playcount(DB_playItem_t *track, uintmax_t count) {

    deadbeef->pl_lock();
    const char *track_location = deadbeef->pl_find_meta(track, LOCATION_TAG);
    deadbeef->pl_unlock();

    // Create the frame if it doesn't exist. Either way set its count.
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

    DB_id3v2_frame_t *updated = id3v2_pcnt_frame_set_count(pcnt, count);

    if (updated != pcnt) {
        // A new frame was created on count update.
        // Remove the old one, add the new one.
        // Should only be one PCNT frame, so: removed == pcnt.
        DB_id3v2_frame_t *removed = id3v2_tag_rem_pcnt_frame(&id3v2);
        if (created) { free(removed); }

        created = 1;
        id3v2_tag_add_frame(&id3v2, updated);
    }

    // Save the changes.
    FILE *actual_file = fopen(track_location, "r+");
    deadbeef->junk_id3v2_write(actual_file, &id3v2);
    fclose(actual_file);

    // Clean up resources.
    if (created) { free(id3v2_tag_rem_pcnt_frame(&id3v2)); }
    deadbeef->junk_id3v2_free(&id3v2);
    deadbeef->fclose(track_file);

    return 0;
}

//
//  Interoperability (meta play_count <---> tag pcnt)
//
static void load_tag_to_meta(DB_playItem_t *track) {
    uintmax_t count = get_track_tag_playcount(track);

    if (count > INT_MAX) {
#ifdef DEBUG
        trace("playcount: tag count is larger than can be displayed\n")
#endif
        count = INT_MAX;
    }
    set_track_meta_playcount(track, count);
}

// Load tag PCNT to meta play_count for all tracks.
static void load_tags_to_meta(void) {
    DB_playItem_t *track = deadbeef->pl_get_first(PL_MAIN);

    while (track) {
        if (is_track_tag_supported(track)) {
            load_tag_to_meta(track);
#ifdef DEBUG
        } else {
            deadbeef->pl_lock();
            const char *location = deadbeef->pl_find_meta(track, LOCATION_TAG);
            deadbeef->pl_unlock();
            trace("playcount: load unsupported: '%s'\n", location)
#endif
        }

        deadbeef->pl_item_unref(track);
        track = deadbeef->pl_get_next(track, PL_MAIN);
    }
}

// Load tag PCNT to meta play_count for tracks without a meta value.
static void load_tags_to_missing_meta(void) {
    DB_playItem_t *track = deadbeef->pl_get_first(PL_MAIN);

    while (track) {
        if (is_track_tag_supported(track)) {
            int count = get_track_meta_playcount(track);
            if (count < 0) {
                load_tag_to_meta(track);
#ifdef DEBUG
                deadbeef->pl_lock();
                const char *location = deadbeef->pl_find_meta(track, LOCATION_TAG);
                deadbeef->pl_unlock();
                trace("playcount: load supported: '%s'\n", location)
#endif
            }
        }

        deadbeef->pl_item_unref(track);
        track = deadbeef->pl_get_next(track, PL_MAIN);
    }
}

static void set_track_playcount(DB_playItem_t *track, int count) {
    set_track_meta_playcount(track, count);
    set_track_tag_playcount(track, count);
}

// Increment track play count (using meta play_count as the authoritative
// value). Ensure the incremented value is valid, and then save to both meta
// and tags.
static void inc_track_playcount(DB_playItem_t *track) {
    int count = get_track_meta_playcount(track);

    if (count < 0) {
        uintmax_t tag_count = get_track_tag_playcount(track);

        if (tag_count < INT_MAX) {
            count = tag_count + 1;

        } else {
            count = INT_MAX;
#ifdef DEBUG
            trace("playcount: tag count is larger than can be displayed\n")
#endif
        }
    } else if (count < INT_MAX) {
        count += 1;
    }

    set_track_playcount(track, count);
}

//
//  Interface Implementation
//
static int start(void) {
    // Note: Plugin will be unloaded if start returns -1.
    return 0;
}

static int connect(void) {
    // Loading tags to meta works in either connect() or on DB_EV_PLUGINSLOADED
    // event, but not in start(). Call here so we can be backwards compatible
    // to API 1.0 instead of 1.5.
    load_tags_to_meta();
    return 0;
}

static int stop(void) {
    return 0;
}

static int reset_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    // When called from the context menu this function is called once per
    // track. The 'void *userdata' is a pointer to the track.
    UNUSED(action)
    set_track_playcount((DB_playItem_t *) userdata, 0);
    return 0;
}

static DB_plugin_action_t reset_playcount_action = {
        .title = "Reset Playcount",
        .name = "reset_playcount",
        .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_MULTIPLE_TRACKS,
        .callback = reset_playcount_callback,
        .next = NULL
};

#ifdef DEBUG
static int increment_playcount_callback(
        struct DB_plugin_action_s *action, void *userdata) {
    UNUSED(action)
    inc_track_playcount((DB_playItem_t *) userdata);
    return 0;
}

static DB_plugin_action_t increment_playcount_action = {
        .title = "Increment Playcount",
        .name = "increment_playcount",
        .flags = DB_ACTION_SINGLE_TRACK | DB_ACTION_MULTIPLE_TRACKS,
        .callback = increment_playcount_callback,
        .next = &reset_playcount_action
};
#endif

static DB_plugin_action_t *get_actions(DB_playItem_t *it) {
    // Metadata is temporary, so only allow it to be displayed/modified if
    // we can actually save its state.
    if (is_track_tag_supported(it)) {
#ifdef DEBUG
        return &increment_playcount_action;
#else
        return &reset_playcount_action;
#endif
    }

    return NULL;
}

static uint32_t previous_event = 0;
static int previous_count = INT_MAX;

static int handle_event(uint32_t current_event, uintptr_t ctx, uint32_t p1, uint32_t p2) {
    UNUSED(p1)
    UNUSED(p2)

    int current_count = deadbeef->pl_getcount(PL_MAIN);

    // We want to increment the play count when we get a song finished event.
    // However we also get these event types when the song is stopped (stop
    // event occurs first).
    if (DB_EV_SONGFINISHED == current_event && DB_EV_STOP != previous_event) {
        ddb_event_track_t *event_track = (ddb_event_track_t *) ctx;
        DB_playItem_t *track = event_track->track;

        if (is_track_tag_supported(track)) { inc_track_playcount(track); }
    }

    // We want to load tags to meta when adding tracks to the player, and save
    // meta to tags when removing tracks from the player.
    //
    // Unfortunately playlist change events don't contain any context and are
    // called by many different actions. We can detect added tracks by using
    // both the event and the increase in song count. There's no easy way to
    // detect removed tracks, so instead we'll save to tags after every meta
    // change (eg. removal from player is same event as deletion from disk..
    // but we don't need to do anything in the latter case).
    else if (DB_EV_PLAYLISTCHANGED == current_event && current_count > previous_count) {
        load_tags_to_missing_meta();
    }

    previous_count = current_count;
    previous_event = current_event;
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
        .connect = connect,
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
