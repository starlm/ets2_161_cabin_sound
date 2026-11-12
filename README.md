# ETS2 1.61 Cabin Sound

Reverses the switch to multiband EQ for cabin buses in 1.62. All vehicle cabins, will use the previous three-band EQ.
Does not affect cabin sounds like blinkers, wipers, etc.

**Note:** Vehicle cabins without a defined three-band EQ will fallback to the multiband EQ.

## Build

**Linux** (MinGW-w64 cross build)

Needs `x86_64-w64-mingw32-g++`, `ninja`, `git`, `cmake`
```bash
./build.sh            # -> build/bin/ets2_161_cabin_sound.dll
```

**Windows**

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Install

Copy `ets2_161_cabin_sound.dll` into `<ETS2>/bin/win_x64/plugins/` while the game is closed.
