
## DeaDBeeF Play Count Plugin

This is a 3rd-party plugin that enables play count tracking for the [DeaDBeeF
audio player](http://deadbeef.sourceforge.net/).


### Building, Installation, Removal

The header file `deadbeef.h` is required for compilation. It can be retrieved
from either the [DeaDBeeF source code](https://github.com/DeaDBeeF-Player/deadbeef),
or is automatically included in the CMake project if DeaDBeeF is installed.

After cloning this repository, execute the following from its source directory
to install the shared library:
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install
```

To remove the plugin, delete any `playcount.so*` files from the local DeaDBeeF
library directory (`~/.local/lib/deadbeef/`).


### Configuration

To display the play count value in the GUI we:
- Right click on the playlist header row, click 'Add Column'.
- Use the following settings:
    - Title: `Play Count`
    - Type: `Custom`
    - Format: `%play_count%`
- Click 'OK'.


### Compatibility

| Deadbeef Version | Plugin Version |
| ---------------: | -------------: |
|            0.7.2 |            1.0 |
|            1.8.2 |            2.0 |

Other versions of Deadbeef Player have not been tested.


### Known Issues

The play count for a track won't be incremented if playback stops after the
current track. This is due to the way that events are produced by Deadbeef
Player.


### Notes

Play counts can only be shown for songs that use the ID3v2 (2.3 or 2.4) tag
format. If play counts are not displayed for songs after the GUI has been
configured try the following steps:

 * Right-click on a song and select 'Track properties' from the context menu.
 * Select the 'Properties' tab.
 * Find the 'Tag Type(s)' key. Its value should contain either "ID3v2.3" or
"ID3v2.4".

If the song does not have the correct tags we can attempt to convert it to use
the correct format:

 * Select the 'Metadata' tab.
 * Click on the 'Settings' button.
 * Ensure that the 'Write ID3v2' option is selected.
 * Click 'Close'.
 * Click 'Apply' to convert the song's tag format.
 * We're done. Click 'Close' to exit the menu.

The song's count will now be updated after it has played (assuming conversion
succeeded). You can optionally close and reopen DeaDBeeF to refresh display of
the play count value.
