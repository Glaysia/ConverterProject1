#!/usr/bin/env python3
"""
Simple helper that records the output of `top -b` while you run a build.

Example
-------
python tools/process_monitor.py \
    --output build_process.log \
    --interval 0.5 \
    --run "cmake --build cmake-build-debug --target all"
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import time
from typing import Optional


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture `top -b` output to a log while optionally executing a build command."
    )
    parser.add_argument(
        "--output",
        default="process_log.txt",
        help="File to store the captured top output (default: process_log.txt).",
    )
    parser.add_argument(
        "--interval",
        type=float,
        default=1.0,
        help="Sampling interval passed to `top -d` (seconds, default: 1.0).",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=None,
        help="Optional duration (seconds) to keep sampling even if no command is provided.",
    )
    parser.add_argument(
        "--run",
        default=None,
        help="Optional shell command to run while capturing (e.g. a cmake --build invocation).",
    )
    parser.add_argument(
        "--width",
        type=int,
        default=512,
        help="Line width passed to `top -w` (default: 512).",
    )
    return parser.parse_args()


def launch_top(args: argparse.Namespace, log_file) -> subprocess.Popen:
    top_cmd = [
        "top",
        "-b",
        "-d",
        str(args.interval),
        "-w",
        str(args.width),
    ]
    return subprocess.Popen(top_cmd, stdout=log_file, stderr=subprocess.STDOUT)


def launch_build(command: str) -> subprocess.Popen:
    # shell=True keeps CLI usage simple; if users want more control they can modify the script.
    return subprocess.Popen(command, shell=True)


def terminate_process(proc: Optional[subprocess.Popen]) -> None:
    if not proc or proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()


def main() -> int:
    args = parse_args()
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    build_proc: Optional[subprocess.Popen] = None
    top_proc: Optional[subprocess.Popen] = None
    start = time.monotonic()

    with open(args.output, "w", encoding="utf-8") as log_file:
        log_file.write(
            f"# top capture started at {time.strftime('%Y-%m-%d %H:%M:%S')} "
            f"(interval={args.interval}s)\n"
        )
        log_file.flush()
        top_proc = launch_top(args, log_file)

        if args.run:
            log_file.write(f"# running command: {args.run}\n")
            log_file.flush()
            build_proc = launch_build(args.run)

        try:
            while True:
                time.sleep(0.2)
                duration_reached = (
                    args.duration is not None and time.monotonic() - start >= args.duration
                )
                build_done = build_proc is not None and build_proc.poll() is not None

                if build_done and args.duration is None:
                    break
                if duration_reached:
                    break
        except KeyboardInterrupt:
            print("Stopping capture (Ctrl+C).", file=sys.stderr)
        finally:
            terminate_process(top_proc)
            terminate_process(build_proc)
            log_file.write("# capture stopped\n")
            log_file.flush()

    return 0


if __name__ == "__main__":
    sys.exit(main())
