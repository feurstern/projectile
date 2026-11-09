# raylib adapter and demos

This directory holds the optional raylib integration for Projectile:

- **The adapter** ([include/projectile/raylib.hpp](include/projectile/raylib.hpp)): header-only
  conversions between Projectile's types and raylib's, and debug drawing for shapes, rays, hits,
  trajectories and simulated projectiles. [docs/integration.md](../../docs/integration.md)
  documents it, with the rest of the engine integration guide.
- **Four demos** ([examples/](examples/)): interactive programs that show the library at work.
  Each one draws through the adapter and uses only Projectile's public API.

Projectile itself never depends on anything here. The core library, its tests and its CI build
without raylib.

| Demo | Shows |
|---|---|
| [ballistic_drop](examples/ballistic_drop/main.cpp) | Balls fired horizontally from a tower under the gravity of Earth, the Moon or Mars, with optional air drag. Trails, the predicted path and landing point of the next shot, and the latest shot's position and velocity. |
| [hitscan_visualizer](examples/hitscan_visualizer/main.cpp) | A movable camera firing rays into a world of planes, boxes, spheres and triangles. Hit points, surface normals, and the coordinates, distance and material of each hit. |
| [material_interaction](examples/material_interaction/main.cpp) | Bullets against glass, wood, a slanted steel plate and concrete: ricochets, penetrations and stops, the energy each impact costs, and material properties that can be changed while the demo runs. |
| [multiple_projectiles](examples/multiple_projectiles/main.cpp) | Thousands of projectiles bouncing around an arena, with the number in flight, the frame rate and the cost of each simulation update. |

## Building

The adapter needs raylib 5.5 or newer; it has been tested with raylib 5.5 and 6.2. An installed
raylib is used if CMake finds one. Otherwise raylib 5.5 is downloaded and built with the demos,
which on Linux needs the X11 and OpenGL development packages. On Ubuntu, these are
`libx11-dev libxcursor-dev libxi-dev libxinerama-dev libxrandr-dev libgl-dev`.

**With Projectile**, from the repository root:

```sh
cmake --preset demos             # Release, with tests, the adapter and the demos
cmake --build --preset demos
build/demos/bin/raylib_ballistic_drop
```

Any configuration can add the demos with `-DPROJECTILE_BUILD_RAYLIB=ON`.

**On their own**, against an installed Projectile (see the README's
[Installed package](../../README.md#installed-package) section):

```sh
cmake -S adapters/raylib -B build/raylib -DCMAKE_PREFIX_PATH=<Projectile's install prefix>
cmake --build build/raylib
build/raylib/bin/raylib_ballistic_drop
```

This build requires exactly the Projectile version of the sources it comes from. It sees
nothing of Projectile but the installed package, which shows that the adapter and the demos
need only the public API. CI builds both ways.

**In your own game**, add the adapter's `include` directory to your include path, or add this
directory with `add_subdirectory()` after Projectile and raylib are available, and link
`Projectile::Raylib`. The adapter is not installed with Projectile.

## Running the demos

Each demo lists its controls at the bottom of its window. All of them share these:

| Key | Action |
|---|---|
| Tab | Switch to the demo's scripted sequence, which repeats, and back |
| Right mouse button, dragged | Turn the camera |
| Mouse wheel | Zoom (orbiting cameras) |
| Esc | Quit |

| Demo | Controls |
|---|---|
| ballistic_drop | Space fire; Up/Down muzzle speed; Left/Right gravity; 1 to 4 gravity of Earth, the Moon, Mars, none; D air drag; C clear |
| hitscan_visualizer | Left click shoot; W, A, S, D move; Q, E down and up; N normals on or off; C clear |
| material_interaction | Space fire; Up/Down elevation; Left/Right speed; 1 to 4 lane and material; for the selected material, `[` `]` ricochet angle, `-` `=` restitution, `,` `.` penetration cost; C clear |
| multiple_projectiles | Hold Space to spray; B burst of 5,000; `+` `-` spawn rate; W collisions on or off; C clear |

## Scripted runs

Every demo can also play its scripted sequence without the keyboard:

```sh
raylib_material_interaction --frames 150            # six seconds at 25 frames per second
raylib_material_interaction --frames 150 --hidden   # in a hidden window
raylib_material_interaction --record frames/        # and save each frame as a PNG
```

A scripted run simulates exactly 1/25 s per frame, however long the frame takes to draw, so it
plays out the same on every machine. At the end, the demo checks that the sequence did what it
should, prints what happened, and exits with code 0 if it did:

| Demo | The scripted run checks that |
|---|---|
| ballistic_drop | Four shots under different gravity all land exactly where, and when, `predict_trajectory()` said; lower gravity means a longer flight |
| hitscan_visualizer | Nine shots hit the objects they were aimed at, and one into the sky hits nothing |
| material_interaction | Bullets ricocheted, penetrated and stopped, and after the wood's penetration cost was raised, the next bullet stopped in it |
| multiple_projectiles | Over 5,000 projectiles flew at once, over 1,000 came to rest, and every update was timed |

With tests enabled, CTest runs each demo this way, as tests labelled `demo`. A demo that finds no
display exits with code 77, and CTest reports its test as skipped. CI runs the demos under Xvfb:

```sh
xvfb-run -a ctest --preset demos -L demo
```

raylib 5.5 crashes instead of failing when it cannot open a window (raylib 6 fixed this). On Linux
and the BSDs, the demos therefore check for the `DISPLAY` environment variable before opening
their window.

## Recording the GIFs

The README's animations are the demos' scripted runs. To record them again:

```sh
python3 adapters/raylib/tools/record_demos.py build/demos --xvfb
```

[tools/record_demos.py](tools/record_demos.py) runs each demo with `--record` and turns the
frames into a GIF with ffmpeg, in `docs/media/`. With `--xvfb`, the demos draw in a virtual display
with Mesa's software renderer, so no windows open and the frames do not depend on the graphics
card. The script needs Python 3.8 or newer, ffmpeg, and for `--xvfb`, `xvfb-run`. Because the
scripted runs are deterministic, recording again reproduces the same scenes. The only exception
is the timings that the multiple_projectiles demo measures and shows.
