#!/usr/bin/env python3
"""Build a labeled MP4 from exact-ROM host frame and sound captures."""

import argparse
import csv
import shutil
import struct
import subprocess
import tempfile
import wave
from pathlib import Path


WIDTH = 1280
HEIGHT = 720
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def run(*command: str) -> None:
    subprocess.run(command, check=True)


def read_csv(path: Path) -> list[tuple[int, int, int]]:
    with path.open(newline="") as source:
        return [tuple(map(int, row)) for row in csv.reader(source)]


def drawtext(text: str, y: int, size: int) -> str:
    escaped = (
        text.replace("\\", "\\\\")
        .replace(":", "\\:")
        .replace("'", "\\'")
        .replace("%", "\\\\%")
    )
    return (
        f"drawtext=fontfile={FONT}:text='{escaped}':fontcolor=white:"
        f"fontsize={size}:x=(w-text_w)/2:y={y}"
    )


def display_filter(version: str, label: str, detail: str) -> str:
    return ",".join(
        [
            "scale=1048:512:flags=neighbor",
            "pad=1280:720:(ow-iw)/2:128:color=0x101820",
            drawtext(f"HP48GX PicoCalc {version} | exact ROM R capture", 35, 30),
            drawtext(label, 82, 25),
            drawtext(detail, 662, 21),
            "format=yuv420p",
        ]
    )


def make_title(path: Path, version: str, seconds: float) -> None:
    filters = ",".join(
        [
            drawtext(f"HP48GX PicoCalc {version}", 205, 42),
            drawtext("Pre-flash graphics and audio validation", 280, 32),
            drawtext("Exact revision-R ROM host capture", 340, 25),
            drawtext("Hardware panel and speaker remain the final check", 430, 21),
            "format=yuv420p",
        ]
    )
    run(
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-f", "lavfi", "-i", f"color=c=0x101820:s={WIDTH}x{HEIGHT}:r=30:d={seconds}",
        "-f", "lavfi", "-i", f"anullsrc=r=44100:cl=mono:d={seconds}",
        "-vf", filters, "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-c:a", "aac", "-b:a", "128k", "-shortest", str(path),
    )


def make_silent_segment(capture: Path, path: Path, start: int, count: int,
                        fps: int, version: str, label: str, detail: str) -> None:
    duration = count / fps
    run(
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-framerate", str(fps), "-start_number", str(start),
        "-i", str(capture / "frame-%05d.pgm"),
        "-f", "lavfi", "-i", f"anullsrc=r=44100:cl=mono:d={duration}",
        "-frames:v", str(count), "-vf", display_filter(version, label, detail),
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-c:a", "aac", "-b:a", "128k", "-shortest", str(path),
    )


def last_audio_burst(edges: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    start = 0
    for index in range(1, len(edges)):
        if edges[index][0] - edges[index - 1][0] > 100_000:
            start = index
    return edges[start:]


def make_audio(path: Path, duration: float, offset: float,
               edges: list[tuple[int, int, int]]) -> None:
    sample_rate = 44_100
    sample_count = int(duration * sample_rate)
    samples = [0] * sample_count
    burst = last_audio_burst(edges)
    if burst:
        base_us = burst[0][0]
        amplitude = int(32767 * 0.60)
        level = 0
        edge_index = 0
        first_sample = int(offset * sample_rate)
        final_sample = min(
            sample_count,
            first_sample + int((burst[-1][0] - base_us) * sample_rate / 1_000_000) + 1,
        )
        for sample in range(first_sample, final_sample):
            elapsed_us = (sample - first_sample) * 1_000_000 / sample_rate
            while edge_index < len(burst) and burst[edge_index][0] - base_us <= elapsed_us:
                level = burst[edge_index][1]
                edge_index += 1
            samples[sample] = amplitude if level else -amplitude

    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def make_demo_segment(capture: Path, path: Path, start: int, count: int,
                      fps: int, version: str, work: Path) -> None:
    frames = read_csv(capture / "frames.csv")
    edges = read_csv(capture / "audio-edges.csv")
    duration = count / fps
    burst = last_audio_burst(edges)
    audio_us = burst[0][0] if burst else frames[start - 1][1]
    nearest_frame = min(frames, key=lambda frame: abs(frame[1] - audio_us))[0]
    audio_offset = max(0.0, (nearest_frame - start) / fps)
    audio = work / "demo.wav"
    make_audio(audio, duration, audio_offset, edges)
    run(
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-framerate", str(fps), "-start_number", str(start),
        "-i", str(capture / "frame-%05d.pgm"), "-i", str(audio),
        "-frames:v", str(count),
        "-vf", display_filter(
            version,
            "1st Demo: setup, beeper, scene changes, and wireframe animation",
            "Audio is reconstructed from captured calculator beeper transitions at 60 percent level",
        ),
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-c:a", "aac", "-b:a", "128k", "-shortest", str(path),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--android-capture", type=Path, required=True)
    parser.add_argument("--demo-capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not shutil.which("ffmpeg"):
        raise SystemExit("ffmpeg is required")

    android_frames = read_csv(args.android_capture / "frames.csv")
    demo_frames = read_csv(args.demo_capture / "frames.csv")
    android_start = max(1, android_frames[-1][0] - 86)
    demo_start = min(160, demo_frames[-1][0])
    args.output.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="hp48-validation-video.") as temporary:
        work = Path(temporary)
        title = work / "00-title.mp4"
        android = work / "01-android.mp4"
        demo = work / "02-demo.mp4"
        make_title(title, args.version, 3.0)
        make_silent_segment(
            args.android_capture, android, android_start,
            android_frames[-1][0] - android_start + 1, 12, args.version,
            "Android 2.01: movement, ENTER+arrow dig chords, and clean scene cut",
            "Changed LCD frames from the same exact-ROM regression used for the UF2",
        )
        make_demo_segment(
            args.demo_capture, demo, demo_start,
            demo_frames[-1][0] - demo_start + 1, 24, args.version, work,
        )
        manifest = work / "segments.txt"
        manifest.write_text(
            "".join(f"file '{part.as_posix()}'\n" for part in (title, android, demo)),
            encoding="utf-8",
        )
        run(
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
            "-f", "concat", "-safe", "0", "-i", str(manifest),
            "-r", "30", "-c:v", "libx264", "-preset", "medium",
            "-crf", "18", "-c:a", "aac", "-b:a", "128k",
            "-movflags", "+faststart", str(args.output),
        )


if __name__ == "__main__":
    main()
