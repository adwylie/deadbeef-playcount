
### DeaDBeeF Play Count Plugin

This is a 3rd-party plugin that enables play count tracking for the [DeaDBeeF
audio player](http://deadbeef.sourceforge.net/). It currently supports updating
ID3v2 tags (2.3, 2.4) for MP3 files.


#### Building, Installation, Removal

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
