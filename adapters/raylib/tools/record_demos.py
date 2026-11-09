#!/usr/bin/env python3
"""Records the raylib demos' scripted runs as the GIFs that the README shows.

    record_demos.py BUILD_DIR [--output DIR] [--xvfb] [--demo NAME ...] [--width PIXELS]

BUILD_DIR is a build of Projectile with the demos, such as build/demos from the demos preset.
Each demo plays its scripted sequence with --record, which saves every frame as a PNG; ffmpeg
then turns the frames into a GIF with a palette of its own (see DEMOS for the settings), written
to docs/media/<demo>.gif unless --output says otherwise. The scripts are deterministic, so the
GIFs show the same scenes on every run.

With --xvfb, the demos run in a virtual X display (xvfb-run) with Mesa's software renderer:
no windows open on the desktop, and the frames do not depend on the graphics card.

Needs Python 3.8 or newer and ffmpeg, plus xvfb-run for --xvfb.
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# How each demo's GIF is encoded: frames per second (25, or 12.5 to keep every other frame), width
# in pixels, and palette size. The scenes are flat-shaded, so they need no dithering. Thousands of
# moving tracers change most pixels in every frame, so that demo's GIF keeps fewer frames and
# pixels to stay near 2 MB.
DEMOS = {
    "ballistic_drop": ("25", 800, 64),
    "hitscan_visualizer": ("25", 800, 64),
    "material_interaction": ("25", 800, 64),
    "multiple_projectiles": ("12.5", 640, 48),
}
REPOSITORY = Path(__file__).resolve().parents[3]


def find_demo(build_dir, name):
    """The demo's executable in a single- or multi-configuration build directory."""
    for directory in (build_dir / "bin", build_dir / "bin" / "Release"):
        for suffix in ("", ".exe"):
            candidate = directory / f"raylib_{name}{suffix}"
            if candidate.is_file():
                return candidate
    sys.exit(f"raylib_{name} not found in {build_dir}; build the demos preset first")


def record(executable, frames_dir, frames, xvfb):
    """Runs the demo's scripted sequence, saving its frames to frames_dir."""
    command = [str(executable), "--record", str(frames_dir), "--frames", str(frames)]
    environment = dict(os.environ)
    if xvfb:
        command = ["xvfb-run", "--auto-servernum", "--server-args=-screen 0 1280x720x24"] + command
        environment["LIBGL_ALWAYS_SOFTWARE"] = "1"
    subprocess.run(command, check=True, env=environment)


def encode(frames_dir, output, settings, width=None):
    """Turns the 25 frames per second in frames_dir into a GIF with a palette of its own."""
    rate, default_width, colors = settings
    filters = (
        f"fps={rate},scale={width or default_width}:-1:flags=lanczos,split[frames][copy];"
        f"[copy]palettegen=max_colors={colors}:stats_mode=diff[palette];"
        "[frames][palette]paletteuse=dither=none:diff_mode=rectangle"
    )
    subprocess.run(
        [
            "ffmpeg", "-y", "-loglevel", "error",
            "-framerate", "25", "-i", str(frames_dir / "frame_%04d.png"),
            "-filter_complex", filters, "-loop", "0", str(output),
        ],
        check=True,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("build_dir", type=Path, help="build directory with the demos")
    parser.add_argument(
        "--output", type=Path, default=REPOSITORY / "docs" / "media",
        help="directory for the GIFs (default: docs/media)",
    )
    parser.add_argument("--xvfb", action="store_true", help="render in a virtual X display")
    parser.add_argument("--demo", action="append", choices=DEMOS, help="record only this demo")
    parser.add_argument("--frames", type=int, default=150, help="frames per demo (default: 150)")
    parser.add_argument("--width", type=int, help="GIF width in pixels (default: per demo)")
    arguments = parser.parse_args()

    for tool in ("ffmpeg",) + (("xvfb-run",) if arguments.xvfb else ()):
        if shutil.which(tool) is None:
            sys.exit(f"{tool} not found")
    arguments.output.mkdir(parents=True, exist_ok=True)

    for name in arguments.demo or DEMOS:
        executable = find_demo(arguments.build_dir, name)
        output = arguments.output / f"{name}.gif"
        with tempfile.TemporaryDirectory(prefix=f"projectile-{name}-") as temporary:
            frames_dir = Path(temporary)
            record(executable, frames_dir, arguments.frames, arguments.xvfb)
            encode(frames_dir, output, DEMOS[name], arguments.width)
        print(f"{output}: {output.stat().st_size / 1e6:.2f} MB")


if __name__ == "__main__":
    main()
