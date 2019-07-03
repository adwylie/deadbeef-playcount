#include <deadbeef.h>

static DB_functions_t *deadbeef;

// TODO: When complete, message on issues #1143, #1812.

// When song has completed playing we want to increase it's play count.
// > detect song has finished event
// > get song ID3v2 info
// > update PCNT frame

// When the user selects the action to reset the play count we want to set play
// count to zero.
// > have action in list of options for song right-click menu
// > detect action selection event
// > get song ID3v2 info
// > update PCNT frame
static int reset_playcount() {
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

static DB_plugin_action_t* get_actions(DB_playItem_t *it) {
    return &reset_playcount_action;
}

// TODO: Should probably be DB_misc_t.
static DB_plugin_t plugin = {

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
        // TODO: Handle message DB_EV_SONGFINISHED to increment playcount.
        .message = NULL,
        .configdialog = NULL
};

extern DB_plugin_t *playcount_load(DB_functions_t *api) {
    deadbeef = api;
    return &plugin;
}
