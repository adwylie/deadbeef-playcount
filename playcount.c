#include <deadbeef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define trace(...) { fprintf(stderr, __VA_ARGS__); }

static DB_functions_t *deadbeef;

static const char *FILE_TYPE_TAG = ":FILETYPE";
static const char *LOCATION_TAG = ":URI";
static const char *METADATA_TYPE_TAG = ":TAGS";

static const char *FILE_TYPE_MP3 = "MP3";
static const char *METADATA_TYPE_ID3V2 = "ID3v2";

// TODO: When complete, message on issues #1143, #1812.
// Main menu item to reset all track play counts to zero?
// or just multi-select all items in a playlist.

// When song has completed playing we want to increase it's play count.
// TODO: call when a song has finished
static int increment_playcount() {
#ifdef DEBUG
    trace("Entered function increment_playcount()\n")
#endif
    // Get the number of selected tracks.
    // TODO: Handle multiple selected tracks.
    int selected_count = deadbeef->pl_getselcount();

    if (selected_count != 1) {
        return selected_count;
    }

    // Since this function is called via the context menu, if there is only
    // one selected item then it must be the same as the cursor item.
    // TODO: Handle selection via search as well. > ddb_playlist_t
    int idx = deadbeef->pl_get_cursor(PL_MAIN);
    DB_playItem_t *track = deadbeef->pl_get_for_idx_and_iter(idx, PL_MAIN);

    if (!deadbeef->pl_is_selected(track)) {
        trace("ERROR: Cursor position is not the selected track\n")
        deadbeef->pl_item_unref(track);
        return 1;
#ifdef DEBUG
    } else {
        deadbeef->pl_lock();
        const char *title = deadbeef->pl_find_meta(track, "title");
        deadbeef->pl_unlock();
        trace("Selected track: %s\n", title)
#endif
    }

    // Read track ID3v2 information (or other tag structure).
    const char *track_file_type = NULL;
    const char *track_location = NULL;
    const char *track_metadata_type = NULL;

    deadbeef->pl_lock();
    DB_metaInfo_t *track_meta = deadbeef->pl_get_metadata_head(track);

    do {
        const char *key = track_meta->key;

        if (0 == strcmp(FILE_TYPE_TAG, key)) {
            track_file_type = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)

        } else if (0 == strcmp(LOCATION_TAG, key)) {
            track_location = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)

        } else if (0 == strcmp(METADATA_TYPE_TAG, key)) {
            track_metadata_type = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)
        }

    } while ((track_meta = track_meta->next) != NULL);
    deadbeef->pl_unlock();

    // Note: API < 1.5 returns 0 for vfs.
    // TODO: Bug? local files start with '/', but that's classified as 'remote'.
    if (0 != strncmp("/", track_location, 1)
            || !deadbeef->is_local_file(track_location)) {
        // Can't update play count for remote audio, no access to metadata.
        // Not technically an error...
        return 0;
    }

    // Get the actual tag structure.
    if (0 == strcmp(FILE_TYPE_MP3, track_file_type)) {

        if (0 != strstr(track_metadata_type, METADATA_TYPE_ID3V2)) {
            // Update ID3v2 PCNT frame, create it if it doesn't exist.
            DB_id3v2_tag_t id3v2;
            memset(&id3v2, 0, sizeof(id3v2));

            DB_FILE *track_file = deadbeef->fopen(track_location);
            if (!track_file) { return -1; }

            int err = deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);
            if (err) { return err; }

            //
            int minor = id3v2.version[0];
            int revision = id3v2.version[1];
            trace("ID3v2.%d.%d\n", minor, revision)

            uint8_t pcnt_found = 0;

            for (DB_id3v2_frame_t *f = id3v2.frames; f; f = f->next) {
                trace("id = %s\n", f->id)

                if (0 == strcmp("PCNT", f->id)) {
                    trace("found PCNT, size: %d bytes\n", f->size)
                    pcnt_found++;

                    // TODO:
                    // set rightmost unset bit (let position be k),
                    // toggle all bits to the right of k
                    (*((uint32_t *) f->data))++;
                }
            }

            DB_id3v2_frame_t *frame = NULL;
            DB_id3v2_frame_t *tail = NULL;

            if (!pcnt_found) {
                trace("didn't find PCNT, making one\n")
                // Create the frame if it wasn't found (with a value of 1).
                // TODO: test frame is not NULL
                frame = malloc(sizeof(DB_id3v2_frame_t) + sizeof(uint32_t));
                memset(frame, 0, sizeof(DB_id3v2_frame_t));

                strcpy(frame->id, "PCNT");
                frame->size = sizeof(uint32_t);

                uint32_t count = 1;
                memcpy(frame->data, &count, frame->size);

                // Add the PCNT frame to the ID3v2 tag.
                for (tail = id3v2.frames; tail && tail->next; tail = tail->next);

                if (tail) {
                    tail->next = frame;
                } else {
                    id3v2.frames = frame;
                }
            }

            // Update PCNT frame/value.
            trace("Writing file\n")
            FILE *actual_file = fopen(track_location, "r+");
            fseek(actual_file, 0, SEEK_SET);
            deadbeef->junk_id3v2_write(actual_file, &id3v2);
            fclose(actual_file);
            trace("Wrote file\n")

            if (frame != NULL) {
                trace("freeing created PCNT frame\n")
                // Remove the PCNT frame from the ID3v2 tag.
                if (tail) {
                    tail->next = NULL;
                } else {
                    id3v2.frames = NULL;
                }

                free(frame);
            }

            deadbeef->junk_id3v2_free(&id3v2);

            deadbeef->fclose(track_file);
        }
    }

#ifdef DEBUG
    // Read to check proper write.
#endif

    // Remove the reference after we're done making changes.
    deadbeef->pl_item_unref(track);
    return 0;
}

// Reset the play count to zero.
// TODO: Do this in a separate thread?
static int reset_playcount() {
#ifdef DEBUG
    trace("Entered function reset_playcount()\n")
#endif
    // Get the number of selected tracks.
    // TODO: Handle multiple selected tracks.
    int selected_count = deadbeef->pl_getselcount();

    if (selected_count != 1) {
        return selected_count;
    }

    // Since this function is called via the context menu, if there is only
    // one selected item then it must be the same as the cursor item.
    // TODO: Handle selection via search as well. > ddb_playlist_t
    int idx = deadbeef->pl_get_cursor(PL_MAIN);
    DB_playItem_t *track = deadbeef->pl_get_for_idx_and_iter(idx, PL_MAIN);

    if (!deadbeef->pl_is_selected(track)) {
        trace("ERROR: Cursor position is not the selected track")
        deadbeef->pl_item_unref(track);
        return 1;
#ifdef DEBUG
    } else {
        deadbeef->pl_lock();
        const char *title = deadbeef->pl_find_meta(track, "title");
        deadbeef->pl_unlock();
        trace("Selected track: %s\n", title)
#endif
    }

    // Read track ID3v2 information (or other tag structure).
    const char *track_file_type = NULL;
    const char *track_location = NULL;
    const char *track_metadata_type = NULL;

    deadbeef->pl_lock();
    DB_metaInfo_t *track_meta = deadbeef->pl_get_metadata_head(track);

    do {
        const char *key = track_meta->key;

        if (0 == strcmp(FILE_TYPE_TAG, key)) {
            track_file_type = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)

        } else if (0 == strcmp(LOCATION_TAG, key)) {
            track_location = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)

        } else if (0 == strcmp(METADATA_TYPE_TAG, key)) {
            track_metadata_type = track_meta->value;
            trace("'%s' is '%s'\n", key, track_meta->value)
        }

    } while ((track_meta = track_meta->next) != NULL);
    deadbeef->pl_unlock();

    // Note: API < 1.5 returns 0 for vfs.
    // TODO: Bug? local files start with '/', but that's classified as 'remote'.
    if (0 != strncmp("/", track_location, 1)
            || !deadbeef->is_local_file(track_location)) {
        // Can't update play count for remote audio, no access to metadata.
        // Not technically an error...
        return 0;
    }

    // Get the actual tag structure.
    if (0 == strcmp(FILE_TYPE_MP3, track_file_type)) {

        if (0 != strstr(track_metadata_type, METADATA_TYPE_ID3V2)) {
            // Update ID3v2 PCNT frame if it exists.
            DB_id3v2_tag_t id3v2;
            memset(&id3v2, 0, sizeof(id3v2));

            DB_FILE *track_file = deadbeef->fopen(track_location);
            if (!track_file) { return -1; }

            int err = deadbeef->junk_id3v2_read_full(track, &id3v2, track_file);
            if (err) { return err; }

            //
            int minor = id3v2.version[0];
            int revision = id3v2.version[1];
            trace("ID3v2.%d.%d\n", minor, revision)

            uint8_t pcnt_found = 0;

            for (DB_id3v2_frame_t *f = id3v2.frames; f; f = f->next) {
                trace("id = %s\n", f->id)

                if (0 == strcmp("PCNT", f->id)) {
                    trace("found PCNT, size: %d bytes\n", f->size)
                    pcnt_found++;

                    uint8_t *data = f->data;
                    for (int i = 0; i < f->size; i++) {
                        data[i] = 0;
                    }
                }
            }

            // Update PCNT frame/value.
            if (pcnt_found) {
                trace("Writing file\n")
                FILE *actual_file = fopen(track_location, "r+");
                fseek(actual_file, 0, SEEK_SET);
                deadbeef->junk_id3v2_write(actual_file, &id3v2);
                fclose(actual_file);
                trace("Wrote file\n")
            }

            deadbeef->junk_id3v2_free(&id3v2);

            deadbeef->fclose(track_file);
        }
    }

#ifdef DEBUG
    // Read to check proper write.
#endif

    // Remove the reference after we're done making changes.
    deadbeef->pl_item_unref(track);

    return 0;
}

// Show play count information in the GUI.
// > ???


static int start() {
    // Note: Plugin will be unloaded if start returns -1.
    return 0;
}

static int stop() {
    return 0;
}

// Add an action in the song context menu,
// allow the play count to be reset.
static DB_plugin_action_t reset_playcount_action = {
        .title = "Reset Playcount",
        .name = "reset_playcount",
        .flags = DB_ACTION_SINGLE_TRACK,
        .callback = reset_playcount,
        .next = NULL
};

#ifdef DEBUG
// Add an action in the song context menu,
// allow the play count to be incremented.
static DB_plugin_action_t increment_playcount_action = {
        .title = "Increment Playcount",
        .name = "increment_playcount",
        .flags = DB_ACTION_SINGLE_TRACK,
        .callback = increment_playcount,
        .next = &reset_playcount_action
};

static DB_plugin_action_t *get_actions(DB_playItem_t *it) {
    return &increment_playcount_action;
}
#else
static DB_plugin_action_t* get_actions(DB_playItem_t *it) {
    return &reset_playcount_action;
}
#endif

// TODO: Handle .message DB_EV_SONGFINISHED to increment playcount.
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
            .message = NULL,
            .configdialog = NULL
        }
};

extern DB_plugin_t *playcount_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin.plugin;
}
