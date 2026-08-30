#!/usr/bin/env python3
"""Build a labeled calculator/CAS MP4 from an exact-ROM host capture."""

import argparse
import bisect
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


def read_numeric_csv(path: Path) -> list[tuple[int, int, int]]:
    with path.open(newline="") as source:
        return [tuple(map(int, row)) for row in csv.reader(source)]


def read_markers(path: Path) -> dict[str, int]:
    with path.open(newline="") as source:
        return {label: int(frame) for label, frame, _ in csv.reader(source)}


def drawtext(text: str, y: int, size: int) -> str:
    escaped = (
        text.replace("\\", "\\\\")
        .replace(":", "\\:")
        .replace("'", "\\'")
        .replace("%", "\\%")
    )
    return (
        f"drawtext=fontfile={FONT}:text='{escaped}':fontcolor=white:"
        f"fontsize={size}:x=(w-text_w)/2:y={y}"
    )


def display_filter(version: str, label: str, detail: str,
                   frame_count: int, hold: float) -> str:
    return ",".join(
        [
            f"trim=end_frame={frame_count}",
            "setpts=PTS-STARTPTS",
            "scale=1048:512:flags=neighbor",
            "pad=1280:720:(ow-iw)/2:128:color=0x101820",
            drawtext(f"HP48GX PicoCalc {version} | exact ROM R capture", 35, 30),
            drawtext(label, 82, 25),
            drawtext(detail, 662, 20),
            f"tpad=stop_mode=clone:stop_duration={hold}",
            "format=yuv420p",
        ]
    )


def make_card(path: Path, version: str, heading: str, detail: str,
              seconds: float) -> None:
    filters = ",".join(
        [
            drawtext(f"HP48GX PicoCalc {version}", 190, 42),
            drawtext(heading, 275, 34),
            drawtext(detail, 350, 23),
            "format=yuv420p",
        ]
    )
    run(
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-f", "lavfi", "-i",
        f"color=c=0x101820:s={WIDTH}x{HEIGHT}:r=30:d={seconds}",
        "-f", "lavfi", "-i", f"anullsrc=r=44100:cl=mono:d={seconds}",
        "-vf", filters, "-c:v", "libx264", "-preset", "medium",
        "-crf", "18", "-c:a", "aac", "-b:a", "128k", "-shortest",
        str(path),
    )


def frame_position(capture_us: int, frames: list[tuple[int, int, int]]) -> float:
    times = [row[1] for row in frames]
    index = bisect.bisect_left(times, capture_us)
    if index <= 0:
        return float(frames[0][0])
    if index >= len(frames):
        return float(frames[-1][0])
    left_frame, left_us, _ = frames[index - 1]
    right_frame, right_us, _ = frames[index]
    fraction = (capture_us - left_us) / max(1, right_us - left_us)
    return left_frame + fraction * (right_frame - left_frame)


def make_audio(path: Path, start: int, end: int, fps: int, hold: float,
               frames: list[tuple[int, int, int]],
               edges: list[tuple[int, int, int]]) -> None:
    sample_rate = 44_100
    duration = (end - start + 1) / fps + hold
    samples = [0] * int(duration * sample_rate)
    amplitude = int(32767 * 0.60)

    groups: list[list[tuple[int, int, int]]] = []
    for edge in edges:
        if not groups or edge[0] - groups[-1][-1][0] > 100_000:
            groups.append([edge])
        else:
            groups[-1].append(edge)

    for group in groups:
        positioned = [
            ((frame_position(edge[0], frames) - start) / fps, edge[1])
            for edge in group
        ]
        for index, (seconds, level) in enumerate(positioned):
            next_seconds = (
                positioned[index + 1][0]
                if index + 1 < len(positioned)
                else seconds + 0.002
            )
            first = max(0, int(seconds * sample_rate))
            final = min(len(samples), int(next_seconds * sample_rate))
            if first < final:
                samples[first:final] = [amplitude if level else -amplitude] * (
                    final - first
                )

    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(struct.pack(f"<{len(samples)}h", *samples))


def make_segment(capture: Path, path: Path, start: int, end: int, fps: int,
                 version: str, label: str, detail: str, hold: float,
                 frames: list[tuple[int, int, int]],
                 edges: list[tuple[int, int, int]], work: Path) -> None:
    count = end - start + 1
    duration = count / fps + hold
    audio = work / f"{path.stem}.wav"
    make_audio(audio, start, end, fps, hold, frames, edges)
    run(
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-framerate", str(fps), "-start_number", str(start),
        "-i", str(capture / "frame-%05d.pgm"), "-i", str(audio),
        "-vf", display_filter(version, label, detail, count, hold),
        "-t", f"{duration:.6f}", "-c:v", "libx264", "-preset", "medium",
        "-crf", "18", "-c:a", "aac", "-b:a", "128k", "-shortest",
        str(path),
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not shutil.which("ffmpeg"):
        raise SystemExit("ffmpeg is required")

    frames = read_numeric_csv(args.capture / "frames.csv")
    edges = read_numeric_csv(args.capture / "audio-edges.csv")
    markers = read_markers(args.capture / "markers.csv")
    args.output.parent.mkdir(parents=True, exist_ok=True)

    segments = []
    if {"merge1_memory_before", "merge1_memory_after"} <= markers.keys():
        segments.append(
            ("merge1_memory_before", "merge1_memory_after", 8,
             "Memory expansion: MERGE1",
             "Virtual 128 KiB Port 1 RAM merged with built-in user memory",
             2.5)
        )
    segments.extend([
        ("cas_factor_input", "cas_factor_result", 40, "CAS 1/4: FACTOR",
         "Factor X^4 - 5*X^2 + 4", 1.5),
        ("cas_factor_result", "cas_diff_result", 42, "CAS 2/4: DIFF",
         "Differentiate X^5 - 3*X^2 + 7*X with respect to X", 1.5),
        ("cas_diff_result", "cas_integrate_result", 46, "CAS 3/4: RISCH",
         "Symbolically integrate 3*X^2 + 2*X + 1", 1.5),
        ("cas_integrate_result", "cas_solve_result", 44, "CAS 4/4: QUAD",
         "Solve X^2 - 5*X + 6 for X", 1.5),
        ("cas_solve_result", "matrix_inverse_result", 34,
         "Matrix 1/2: inverse", "Invert [[1,2],[3,4]]", 1.5),
        ("matrix_inverse_result", "matrix_product_result", 42,
         "Matrix 2/2: multiplication",
         "[[1,2],[3,4]] x [[5,6],[7,8]]", 1.5),
        ("multiple_plot_form", "multiple_plot_result", 24,
         "Plot 1/2: four simultaneous functions",
         "HP TEACH equations ONE through FOUR in radians", 2.0),
        ("parametric_surface_start", "parametric_surface_result", 18,
         "Plot 2/2: 3D parametric surface",
         "HP TEACH PR-SURFACE example, rendered by revision-R ROM", 2.0),
    ])

    with tempfile.TemporaryDirectory(prefix="hp48-calculator-video.") as temporary:
        work = Path(temporary)
        parts: list[Path] = []
        has_merge = segments[0][0] == "merge1_memory_before"
        cards = [
            (("Memory, calculator, CAS, matrix, and graph validation"
              if has_merge else
              "Calculator, CAS, matrix, and graph validation"),
             "Exact ROM execution; captured beeper audio is preserved at 60 percent"),
            ("128 KiB Port 1 expansion",
             "MEM before and after the stock revision-R MERGE1 command"),
            ("Symbolic algebra (CAS)",
             "Factor, differentiate, integrate, and solve"),
            ("Matrix operations", "2x2 inverse and matrix multiplication"),
            ("2D and 3D graphing",
             "Four functions followed by a parametric surface"),
        ]
        cas_index = 1 if has_merge else 0
        matrix_index = cas_index + 4
        plot_index = matrix_index + 2
        card_positions = {0: cards[0:2] if has_merge else cards[0:1]}
        card_positions.setdefault(cas_index, []).append(cards[2])
        card_positions.setdefault(matrix_index, []).append(cards[3])
        card_positions.setdefault(plot_index, []).append(cards[4])
        part_number = 0
        for index, segment in enumerate(segments):
            for heading, detail in card_positions.get(index, []):
                card = work / f"{part_number:02d}-card.mp4"
                make_card(card, args.version, heading, detail, 2.5)
                parts.append(card)
                part_number += 1
            start_label, end_label, fps, label, detail, hold = segment
            start = markers[start_label]
            if start_label in {
                "cas_factor_result", "cas_diff_result",
                "cas_integrate_result", "cas_solve_result",
                "matrix_inverse_result",
            }:
                start = min(markers[end_label], start + 10)
            clip = work / f"{part_number:02d}-capture.mp4"
            make_segment(
                args.capture, clip, start, markers[end_label], fps,
                args.version, label, detail, hold, frames, edges, work,
            )
            parts.append(clip)
            part_number += 1

        manifest = work / "segments.txt"
        manifest.write_text(
            "".join(f"file '{part.as_posix()}'\n" for part in parts),
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
