#!/usr/bin/env python3
"""Convert binary 8-bit PGM images to dependency-free grayscale PNG files."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def read_token(stream) -> bytes:
    while True:
        byte = stream.read(1)
        if not byte:
            raise ValueError("unexpected end of PGM header")
        if byte == b"#":
            stream.readline()
        elif not byte.isspace():
            token = byte
            break

    while True:
        byte = stream.read(1)
        if not byte or byte.isspace():
            return token
        token += byte


def png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type)
    checksum = zlib.crc32(payload, checksum)
    return (
        struct.pack(">I", len(payload))
        + chunk_type
        + payload
        + struct.pack(">I", checksum & 0xFFFFFFFF)
    )


def convert(source: Path, output_directory: Path) -> Path:
    with source.open("rb") as stream:
        if read_token(stream) != b"P5":
            raise ValueError(f"{source}: expected binary P5 PGM")
        width = int(read_token(stream))
        height = int(read_token(stream))
        maximum = int(read_token(stream))
        if width <= 0 or height <= 0 or maximum != 255:
            raise ValueError(f"{source}: expected positive dimensions and maxval 255")
        pixels = stream.read()

    expected_bytes = width * height
    if len(pixels) != expected_bytes:
        raise ValueError(
            f"{source}: expected {expected_bytes} pixel bytes, got {len(pixels)}"
        )

    scanlines = b"".join(
        b"\x00" + pixels[row * width : (row + 1) * width]
        for row in range(height)
    )
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    output_directory.mkdir(parents=True, exist_ok=True)
    output = output_directory / f"{source.stem}.png"
    output.write_bytes(
        PNG_SIGNATURE
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(scanlines, level=9))
        + png_chunk(b"IEND", b"")
    )
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-directory", required=True, type=Path)
    parser.add_argument("--remove-source", action="store_true")
    parser.add_argument("images", nargs="+", type=Path)
    arguments = parser.parse_args()
    for image in arguments.images:
        output = convert(image, arguments.output_directory)
        if arguments.remove_source:
            image.unlink()
        print(output)


if __name__ == "__main__":
    main()
