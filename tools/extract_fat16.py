#!/usr/bin/env python3
"""Extract a FAT12/FAT16 partition from a raw MBR disk image.

The tool is intentionally read-only with respect to the source image.  It is
small enough to audit and supports the DOS 8.3 names used by the source disk,
plus long-file-name entries when present.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path


@dataclass
class Entry:
    path: str
    size: int
    cluster: int
    attributes: int
    sha256: str | None = None


class FatVolume:
    def __init__(self, image: Path, partition_index: int = 0) -> None:
        self.image_path = image
        self.file = image.open("rb")
        mbr = self._read_at(0, 512)
        if mbr[510:512] != b"\x55\xaa":
            raise ValueError("image does not contain a valid MBR signature")

        entry_offset = 446 + partition_index * 16
        partition = mbr[entry_offset : entry_offset + 16]
        self.partition_type = partition[4]
        self.partition_lba, self.partition_sectors = struct.unpack_from(
            "<II", partition, 8
        )
        if not self.partition_lba or not self.partition_sectors:
            raise ValueError(f"partition {partition_index} is empty")

        self.partition_offset = self.partition_lba * 512
        boot = self._read_at(self.partition_offset, 512)
        if boot[510:512] != b"\x55\xaa":
            raise ValueError("partition does not contain a valid boot sector")

        (
            self.bytes_per_sector,
            self.sectors_per_cluster,
            self.reserved_sectors,
            self.fat_count,
            self.root_entry_count,
            total_sectors_16,
            _media,
            self.sectors_per_fat,
        ) = struct.unpack_from("<HBHBHHBH", boot, 11)
        total_sectors_32 = struct.unpack_from("<I", boot, 32)[0]
        self.total_sectors = total_sectors_16 or total_sectors_32
        self.volume_label = boot[43:54].decode("ascii", "replace").rstrip()
        self.fs_label = boot[54:62].decode("ascii", "replace").rstrip()

        self.root_dir_sectors = (
            self.root_entry_count * 32 + self.bytes_per_sector - 1
        ) // self.bytes_per_sector
        self.fat_offset = self.partition_offset + (
            self.reserved_sectors * self.bytes_per_sector
        )
        self.root_offset = self.partition_offset + (
            self.reserved_sectors + self.fat_count * self.sectors_per_fat
        ) * self.bytes_per_sector
        self.data_offset = self.root_offset + (
            self.root_dir_sectors * self.bytes_per_sector
        )
        data_sectors = self.total_sectors - (
            self.reserved_sectors
            + self.fat_count * self.sectors_per_fat
            + self.root_dir_sectors
        )
        self.cluster_count = data_sectors // self.sectors_per_cluster
        self.fat_bits = 12 if self.cluster_count < 4085 else 16
        if self.fat_bits not in (12, 16):
            raise ValueError("only FAT12 and FAT16 are supported")
        self.fat = self._read_at(
            self.fat_offset, self.sectors_per_fat * self.bytes_per_sector
        )

    def close(self) -> None:
        self.file.close()

    def _read_at(self, offset: int, size: int) -> bytes:
        self.file.seek(offset)
        data = self.file.read(size)
        if len(data) != size:
            raise EOFError(f"short read at byte {offset}: wanted {size}, got {len(data)}")
        return data

    def _next_cluster(self, cluster: int) -> int:
        if self.fat_bits == 16:
            return struct.unpack_from("<H", self.fat, cluster * 2)[0]
        offset = cluster + cluster // 2
        pair = struct.unpack_from("<H", self.fat, offset)[0]
        return (pair >> 4) if cluster & 1 else (pair & 0x0FFF)

    def _is_eoc(self, cluster: int) -> bool:
        return cluster >= (0xFFF8 if self.fat_bits == 16 else 0xFF8)

    def read_chain(self, start_cluster: int, size: int | None = None) -> bytes:
        if start_cluster < 2:
            return b""
        cluster_size = self.bytes_per_sector * self.sectors_per_cluster
        chunks: list[bytes] = []
        cluster = start_cluster
        seen: set[int] = set()
        while not self._is_eoc(cluster):
            if cluster in seen:
                raise ValueError(f"cluster loop detected at {cluster}")
            if cluster < 2 or cluster >= self.cluster_count + 2:
                raise ValueError(f"invalid cluster {cluster}")
            seen.add(cluster)
            offset = self.data_offset + (cluster - 2) * cluster_size
            chunks.append(self._read_at(offset, cluster_size))
            cluster = self._next_cluster(cluster)
        result = b"".join(chunks)
        return result[:size] if size is not None else result

    @staticmethod
    def _short_name(raw: bytes) -> str:
        base = raw[:8].decode("cp437", "replace").rstrip()
        ext = raw[8:11].decode("cp437", "replace").rstrip()
        return f"{base}.{ext}" if ext else base

    @staticmethod
    def _lfn_part(raw: bytes) -> str:
        units = raw[1:11] + raw[14:26] + raw[28:32]
        chars: list[str] = []
        for index in range(0, len(units), 2):
            value = struct.unpack_from("<H", units, index)[0]
            if value in (0x0000, 0xFFFF):
                break
            chars.append(chr(value))
        return "".join(chars)

    def _directory_bytes(self, cluster: int | None) -> bytes:
        if cluster is None:
            return self._read_at(
                self.root_offset, self.root_entry_count * 32
            )
        return self.read_chain(cluster)

    def walk(self, cluster: int | None = None, prefix: Path = Path()) -> list[tuple[Entry, bytes | None]]:
        raw_dir = self._directory_bytes(cluster)
        output: list[tuple[Entry, bytes | None]] = []
        lfn_parts: list[tuple[int, str]] = []
        for offset in range(0, len(raw_dir), 32):
            raw = raw_dir[offset : offset + 32]
            if len(raw) < 32 or raw[0] == 0x00:
                break
            if raw[0] == 0xE5:
                lfn_parts.clear()
                continue
            attributes = raw[11]
            if attributes == 0x0F:
                lfn_parts.append((raw[0] & 0x1F, self._lfn_part(raw)))
                continue
            if attributes & 0x08:
                lfn_parts.clear()
                continue

            short_name = self._short_name(raw[:11])
            name = "".join(part for _, part in sorted(lfn_parts)) if lfn_parts else short_name
            lfn_parts.clear()
            if name in (".", ".."):
                continue
            start_cluster = struct.unpack_from("<H", raw, 26)[0]
            size = struct.unpack_from("<I", raw, 28)[0]
            relative = prefix / name
            entry = Entry(relative.as_posix(), size, start_cluster, attributes)
            if attributes & 0x10:
                output.append((entry, None))
                output.extend(self.walk(start_cluster, relative))
            else:
                data = self.read_chain(start_cluster, size)
                entry.sha256 = hashlib.sha256(data).hexdigest()
                output.append((entry, data))
        return output

    def metadata(self) -> dict[str, int | str]:
        return {
            "image": str(self.image_path),
            "partition_type": self.partition_type,
            "partition_lba": self.partition_lba,
            "partition_sectors": self.partition_sectors,
            "bytes_per_sector": self.bytes_per_sector,
            "sectors_per_cluster": self.sectors_per_cluster,
            "fat_bits": self.fat_bits,
            "volume_label": self.volume_label,
            "filesystem_label": self.fs_label,
        }


def safe_destination(root: Path, relative: str) -> Path:
    destination = (root / Path(relative)).resolve()
    root = root.resolve()
    if destination != root and root not in destination.parents:
        raise ValueError(f"unsafe output path in image: {relative}")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--partition", type=int, default=0)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()

    volume = FatVolume(args.image, args.partition)
    try:
        entries = volume.walk()
        args.output.mkdir(parents=True, exist_ok=True)
        for entry, data in entries:
            destination = safe_destination(args.output, entry.path)
            if data is None:
                destination.mkdir(parents=True, exist_ok=True)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
        manifest = {
            "volume": volume.metadata(),
            "entries": [asdict(entry) for entry, _ in entries],
        }
    finally:
        volume.close()

    manifest_path = args.manifest or args.output.with_suffix(".manifest.json")
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    file_count = sum(1 for _, data in entries if data is not None)
    directory_count = sum(1 for _, data in entries if data is None)
    print(f"extracted {file_count} files and {directory_count} directories")
    print(f"manifest: {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
