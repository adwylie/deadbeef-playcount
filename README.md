
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
- Right click on the playlist header row, click "Add Column".
- Use the following settings:
    - Title: `Play Count`
    - Type: `Custom`
    - Format: `%play_count%`
- Click "OK".
