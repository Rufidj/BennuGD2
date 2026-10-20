from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass, field
from pathlib import PurePosixPath, PureWindowsPath
from typing import Iterable


EXTENSION_RE = re.compile(
    r"\.(jpg|jpeg|png|gif|bmp|tif|tiff|heic|webp|mp4|mov|avi|mkv|mts|3gp|mpg|mpeg)$",
    re.IGNORECASE,
)
WINDOWS_PATH_RE = re.compile(r'([A-Za-z]:\\[^:*?"<>|\r\n]+(?:\\[^:*?"<>|\r\n]+)*)')
POSIX_PATH_RE = re.compile(r'((?:/|\\\$Root[\\/])[^:*?"<>|\r\n]+(?:[\\/][^:*?"<>|\r\n]+)*)')
QUOTED_PATH_RE = re.compile(r'"([^"\n]+(?:\\|/)[^"\n]+)"')
EXTRACTED_NAME_RE = re.compile(r"off0x([0-9a-fA-F]+)_size(\d+)")
DEV_BY_ID_RE = re.compile(r"(/dev/disk/by-id/[^:\s]+)")
USB_ID_RE = re.compile(r"usb-([A-Za-z0-9_]+)_([A-Za-z0-9]+)-\d+$")
MAX_RECORDS_PER_GROUP = 5000


def _build_signature_result(
    label: str,
    extension: str,
    signature_offset: int = 0,
    content_size: int = 0,
) -> dict[str, int | str]:
    return {
        "label": label,
        "extension": extension,
        "signature_offset": signature_offset,
        "content_size": content_size,
    }


def detect_file_signature(data: bytes) -> dict[str, str]:
    head4k = data[:4096]
    sample = data[:8 * 1024 * 1024]

    jpeg_pos = sample.find(b"\xff\xd8\xff")
    if jpeg_pos != -1:
        jpeg_end = sample.find(b"\xff\xd9", jpeg_pos + 2)
        jpeg_size = (jpeg_end + 2 - jpeg_pos) if jpeg_end != -1 else 0
        return _build_signature_result("jpeg_probable", ".jpg", jpeg_pos, jpeg_size)
    png_pos = sample.find(b"\x89PNG\r\n\x1a\n")
    if png_pos != -1:
        png_end = sample.find(b"IEND\xaeB`\x82", png_pos + 8)
        png_size = (png_end + 8 - png_pos) if png_end != -1 else 0
        return _build_signature_result("png_probable", ".png", png_pos, png_size)
    gif87_pos = sample.find(b"GIF87a")
    gif89_pos = sample.find(b"GIF89a")
    gif_candidates = [pos for pos in (gif87_pos, gif89_pos) if pos != -1]
    if gif_candidates:
        gif_pos = min(gif_candidates)
        gif_end = sample.find(b"\x3b", gif_pos + 6)
        gif_size = (gif_end + 1 - gif_pos) if gif_end != -1 else 0
        return _build_signature_result("gif_probable", ".gif", gif_pos, gif_size)
    tiff_ii_pos = sample.find(b"II*\x00")
    tiff_mm_pos = sample.find(b"MM\x00*")
    tiff_candidates = [pos for pos in (tiff_ii_pos, tiff_mm_pos) if pos != -1]
    if tiff_candidates:
        return _build_signature_result("tiff_probable", ".tif", min(tiff_candidates), 0)
    bmp_pos = head4k[:128].find(b"BM")
    if bmp_pos != -1:
        bmp_size = 0
        if bmp_pos + 6 <= len(sample):
            bmp_size = int.from_bytes(sample[bmp_pos + 2:bmp_pos + 6], "little", signed=False)
        return _build_signature_result("bmp_probable", ".bmp", bmp_pos, bmp_size)
    if data.startswith(b"RIFF") and len(data) >= 12 and data[8:12] == b"WEBP":
        webp_size = int.from_bytes(data[4:8], "little", signed=False) + 8 if len(data) >= 8 else 0
        return _build_signature_result("webp_probable", ".webp", 0, webp_size)
    riff_pos = sample.find(b"RIFF")
    if riff_pos != -1 and riff_pos + 12 <= len(sample):
        riff_kind = sample[riff_pos + 8:riff_pos + 12]
        if riff_kind == b"WEBP":
            webp_size = int.from_bytes(sample[riff_pos + 4:riff_pos + 8], "little", signed=False) + 8
            return _build_signature_result("webp_probable", ".webp", riff_pos, webp_size)
        if riff_kind == b"AVI ":
            avi_size = int.from_bytes(sample[riff_pos + 4:riff_pos + 8], "little", signed=False) + 8
            return _build_signature_result("avi_probable", ".avi", riff_pos, avi_size)
    pdf_pos = sample.find(b"%PDF")
    if pdf_pos != -1:
        pdf_end = sample.rfind(b"%%EOF")
        pdf_size = (pdf_end + 5 - pdf_pos) if pdf_end != -1 and pdf_end > pdf_pos else 0
        return _build_signature_result("pdf_probable", ".pdf", pdf_pos, pdf_size)
    zip_pos = sample.find(b"PK\x03\x04")
    if zip_pos != -1:
        zip_end = sample.rfind(b"PK\x05\x06")
        zip_size = (zip_end + 22 - zip_pos) if zip_end != -1 and zip_end > zip_pos else 0
        return _build_signature_result("zip_probable", ".zip", zip_pos, zip_size)
    ftyp_pos = sample.find(b"ftyp")
    if ftyp_pos != -1 and ftyp_pos + 8 <= len(sample):
        container_pos = max(0, ftyp_pos - 4)
        atom_size = 0
        if container_pos + 4 <= len(sample):
            atom_size = int.from_bytes(sample[container_pos:container_pos + 4], "big", signed=False)
        major = sample[ftyp_pos + 4:ftyp_pos + 8]
        if major in {b"qt  ", b"moov"}:
            return _build_signature_result("mov_probable", ".mov", container_pos, atom_size)
        if major in {b"heic", b"heix", b"hevc", b"hevx", b"mif1", b"msf1"}:
            return _build_signature_result("heic_probable", ".heic", container_pos, atom_size)
        if major in {b"3gp4", b"3gp5", b"3gp6", b"3gg6", b"3g2a", b"3g2b"}:
            return _build_signature_result("3gp_probable", ".3gp", container_pos, atom_size)
        return _build_signature_result("mp4_probable", ".mp4", container_pos, atom_size)
    mkv_pos = sample.find(b"\x1a\x45\xdf\xa3")
    if mkv_pos != -1:
        return _build_signature_result("mkv_probable", ".mkv", mkv_pos, 0)
    mpeg_pack_pos = sample.find(b"\x00\x00\x01\xba")
    mpeg_seq_pos = sample.find(b"\x00\x00\x01\xb3")
    mpeg_candidates = [pos for pos in (mpeg_pack_pos, mpeg_seq_pos) if pos != -1]
    if mpeg_candidates:
        return _build_signature_result("mpeg_probable", ".mpg", min(mpeg_candidates), 0)
    mp3_pos = sample.find(b"ID3")
    if mp3_pos != -1:
        return _build_signature_result("mp3_probable", ".mp3", mp3_pos, 0)
    if len(sample) >= 564 and sample[0] == 0x47 and sample[188] == 0x47 and sample[376] == 0x47:
        return _build_signature_result("mts_probable", ".mts", 0, 0)
    xml_pos = sample[:65536].find(b"<?xml")
    if xml_pos != -1:
        return _build_signature_result("xml_probable", ".xml", xml_pos, 0)
    return _build_signature_result("unknown_signature", ".bin", 0, 0)


def detect_with_file_command(file_path: str) -> dict[str, str]:
    try:
        result = subprocess.run(
            ["file", "--brief", file_path],
            capture_output=True,
            text=True,
            check=True,
        )
        description = result.stdout.strip()
    except (subprocess.SubprocessError, FileNotFoundError):
        return {"label": "unknown_signature", "extension": ".bin", "description": ""}

    lowered = description.lower()
    if "mpeg" in lowered:
        return {"label": "mpeg_probable", "extension": ".mpg", "description": description}
    if "jpeg" in lowered:
        return {"label": "jpeg_probable", "extension": ".jpg", "description": description}
    if "png" in lowered:
        return {"label": "png_probable", "extension": ".png", "description": description}
    if "gif" in lowered:
        return {"label": "gif_probable", "extension": ".gif", "description": description}
    if "tiff" in lowered:
        return {"label": "tiff_probable", "extension": ".tif", "description": description}
    if "web/p" in lowered or "webp" in lowered:
        return {"label": "webp_probable", "extension": ".webp", "description": description}
    if "heic" in lowered or "heif" in lowered:
        return {"label": "heic_probable", "extension": ".heic", "description": description}
    if "pdf" in lowered:
        return {"label": "pdf_probable", "extension": ".pdf", "description": description}
    if "avi" in lowered:
        return {"label": "avi_probable", "extension": ".avi", "description": description}
    if "matroska" in lowered or "webm" in lowered:
        return {"label": "mkv_probable", "extension": ".mkv", "description": description}
    if "quicktime" in lowered or "mov" in lowered:
        return {"label": "mov_probable", "extension": ".mov", "description": description}
    if "mp4" in lowered or "iso media" in lowered:
        return {"label": "mp4_probable", "extension": ".mp4", "description": description}
    if "3gpp" in lowered:
        return {"label": "3gp_probable", "extension": ".3gp", "description": description}
    if "mp3" in lowered or "mpeg layer iii" in lowered or "audio file with id3" in lowered:
        return {"label": "mp3_probable", "extension": ".mp3", "description": description}
    if "transport stream" in lowered:
        return {"label": "mts_probable", "extension": ".mts", "description": description}
    if "zip" in lowered:
        return {"label": "zip_probable", "extension": ".zip", "description": description}
    if "openpgp public key" in lowered:
        return {"label": "openpgp_probable", "extension": ".pgp", "description": description}
    if "xml" in lowered:
        return {"label": "xml_probable", "extension": ".xml", "description": description}
    return {"label": "unknown_signature", "extension": ".bin", "description": description}


@dataclass
class LogEntry:
    path: str
    line_number: int
    raw_line: str
    is_file: bool
    source_kind: str = "path"


@dataclass
class TreeNode:
    name: str
    path: str
    is_file: bool = False
    size_bytes: int = 0
    children: dict[str, "TreeNode"] = field(default_factory=dict)
    raw_lines: list[str] = field(default_factory=list)
    line_numbers: list[int] = field(default_factory=list)

    def add_child(self, child: "TreeNode") -> None:
        self.children[child.name] = child


class DmdeLogParser:
    def parse_file(self, file_path: str) -> list[LogEntry]:
        entries: list[LogEntry] = []
        with open(file_path, "r", encoding="utf-8", errors="replace") as handle:
            for line_number, raw_line in enumerate(handle, start=1):
                entries.extend(self._parse_line(raw_line.rstrip("\n"), line_number))
        return self._dedupe(entries)

    def _parse_line(self, line: str, line_number: int) -> list[LogEntry]:
        if line.startswith("scan_dev="):
            return [
                LogEntry(
                    path=line.split(":", 7)[-1],
                    line_number=line_number,
                    raw_line=line,
                    is_file=False,
                    source_kind="device",
                )
            ]

        candidates: list[str] = []
        candidates.extend(match.group(1) for match in WINDOWS_PATH_RE.finditer(line))
        candidates.extend(match.group(1) for match in POSIX_PATH_RE.finditer(line))
        candidates.extend(match.group(1) for match in QUOTED_PATH_RE.finditer(line))

        if not candidates and any(token in line for token in ("/$Root/", "\\$Root\\", "/Root/", "\\Root\\")):
            candidates.extend(self._extract_breadcrumbs(line))

        entries: list[LogEntry] = []
        for candidate in candidates:
            normalized = self._normalize_path(candidate)
            if not normalized or normalized in ("/", "\\"):
                continue
            entries.append(
                LogEntry(
                    path=normalized,
                    line_number=line_number,
                    raw_line=line,
                    is_file=self._looks_like_file(normalized),
                    source_kind="path",
                )
            )
        return entries

    def _extract_breadcrumbs(self, line: str) -> list[str]:
        cleaned = line.replace("\\", "/")
        tokens = [segment.strip() for segment in cleaned.split() if "/" in segment]
        return [token.strip("[](){}<>,;") for token in tokens]

    def _normalize_path(self, path: str) -> str:
        cleaned = path.strip().strip("'").strip('"')
        cleaned = cleaned.replace("\\", "/")
        cleaned = re.sub(r"/+", "/", cleaned)

        if re.match(r"^[A-Za-z]:/", cleaned):
            return str(PureWindowsPath(cleaned).as_posix())
        if cleaned.startswith("/"):
            return str(PurePosixPath(cleaned))
        if cleaned.startswith("$Root/"):
            return f"/{cleaned}"
        if cleaned.startswith("/$Root/") or cleaned.startswith("/Root/"):
            return cleaned
        return ""

    def _looks_like_file(self, path: str) -> bool:
        return bool(EXTENSION_RE.search(path.rsplit("/", 1)[-1]))

    def _dedupe(self, entries: Iterable[LogEntry]) -> list[LogEntry]:
        seen: set[tuple[str, int]] = set()
        unique: list[LogEntry] = []
        for entry in entries:
            key = (entry.path, entry.line_number)
            if key in seen:
                continue
            seen.add(key)
            unique.append(entry)
        return unique


class EntryTreeBuilder:
    def build(self, entries: list[LogEntry]) -> TreeNode:
        root = TreeNode(name="/", path="/", is_file=False)
        for entry in entries:
            parts = [part for part in entry.path.split("/") if part]
            if not parts:
                continue
            current = root
            current_path = ""
            for index, part in enumerate(parts):
                current_path = f"{current_path}/{part}"
                is_leaf = index == len(parts) - 1
                child = current.children.get(part)
                if child is None:
                    child = TreeNode(
                        name=part,
                        path=current_path,
                        is_file=is_leaf and entry.is_file,
                        size_bytes=0,
                    )
                    current.add_child(child)
                if is_leaf:
                    child.is_file = entry.is_file
                    child.raw_lines.append(entry.raw_line)
                    child.line_numbers.append(entry.line_number)
                    source_path = self._source_file_path_for_node(child)
                    if source_path and os.path.isfile(source_path):
                        try:
                            child.size_bytes = os.path.getsize(source_path)
                        except OSError:
                            child.size_bytes = 0
                current = child
        return root

    def _source_file_path_for_node(self, _node: TreeNode) -> str:
        return ""


class DmdeScanStateParser:
    def parse_assignments(self, file_path: str) -> dict[str, str]:
        assignments: dict[str, str] = {}
        current_key: str | None = None
        current_parts: list[str] = []

        with open(file_path, "r", encoding="utf-8", errors="replace") as handle:
            for raw_line in handle:
                line = raw_line.rstrip("\n")
                if current_key is not None:
                    continued = line.endswith("\\")
                    current_parts.append(line[:-1] if continued else line)
                    if not continued:
                        assignments[current_key] = "".join(current_parts).strip().strip('"')
                        current_key = None
                        current_parts = []
                    continue

                if "=" not in line or line.startswith("[") or line.startswith(";"):
                    continue
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip()
                continued = value.endswith("\\")
                if continued:
                    current_key = key
                    current_parts = [value[:-1]]
                else:
                    assignments[key] = value.strip().strip('"')
        return assignments

    def build_summary(self, assignments: dict[str, str]) -> dict:
        extarr = self._parse_extarr(assignments.get("extarr", ""))
        enabled_types = self._parse_enabled_types(assignments)
        scan_range = assignments.get("scan_range", "").strip('"')
        range_start, range_end = self._parse_scan_range(scan_range)
        blkv = self._parse_blkv(assignments, range_end)
        volumes = self._parse_count_prefix(assignments.get("volumes", ""))
        indx_runs = self._parse_count_prefix(assignments.get("INDX_ofs", ""))
        return {
            "scan_dev": assignments.get("scan_dev", ""),
            "scan_range": scan_range,
            "scan_range_start": range_start,
            "scan_range_end": range_end,
            "volume_candidates": volumes,
            "indx_offsets": indx_runs,
            "signature_types_total": len(extarr),
            "signature_types_enabled": enabled_types,
            "raw_block_groups": blkv,
        }

    def _parse_extarr(self, raw_value: str) -> list[str]:
        if ";" not in raw_value:
            return []
        _, rest = raw_value.split(";", 1)
        return [token.strip() for token in rest.split(",") if token.strip()]

    def _parse_enabled_types(self, assignments: dict[str, str]) -> list[str]:
        enabled: list[str] = []
        for key, value in assignments.items():
            if not key.startswith("ft_op["):
                continue
            token = key[6:-1]
            try:
                parsed = int(value, 16)
            except ValueError:
                continue
            if parsed != 0:
                enabled.append(token)
        return sorted(enabled)

    def _parse_blkv(self, assignments: dict[str, str], range_end: int) -> list[dict]:
        result: list[dict] = []
        for key, value in assignments.items():
            if not key.startswith("blkv["):
                continue
            count = self._parse_count_prefix(value)
            records = self._parse_records_after_prefix(value)
            preview_records = records[:MAX_RECORDS_PER_GROUP]
            decoded_records = [self._decode_block_record(record, range_end) for record in preview_records]
            result.append(
                {
                    "group": key,
                    "declared_count": count,
                    "records_parsed": len(records),
                    "records": preview_records,
                    "decoded_records": decoded_records,
                }
            )
        result.sort(key=lambda item: item["group"])
        return result

    def _parse_scan_range(self, raw_value: str) -> tuple[int, int]:
        if "-" not in raw_value:
            return 0, 0
        start_text, end_text = raw_value.split("-", 1)
        return self._parse_int(start_text), self._parse_int(end_text)

    def _parse_count_prefix(self, raw_value: str) -> int:
        if ";" not in raw_value:
            return 0
        prefix, _ = raw_value.split(";", 1)
        try:
            return int(prefix, 0)
        except ValueError:
            return 0

    def _parse_records_after_prefix(self, raw_value: str) -> list[list[str]]:
        if ";" not in raw_value:
            return []
        _, rest = raw_value.split(";", 1)
        records: list[list[str]] = []
        for entry in rest.split(","):
            entry = entry.strip()
            if not entry:
                continue
            records.append([field.strip() for field in entry.split(":")])
        return records

    def _decode_block_record(self, record: list[str], range_end: int) -> dict:
        decoded = {
            "raw_fields": record,
            "start_offset_hex": record[0] if len(record) > 0 else "",
            "block_size": self._parse_int(record[1]) if len(record) > 1 else 0,
            "field_2": self._parse_int(record[2]) if len(record) > 2 else 0,
            "field_3": self._parse_int(record[3]) if len(record) > 3 else 0,
            "field_4": self._parse_int(record[4]) if len(record) > 4 else 0,
            "field_5": self._parse_int(record[5]) if len(record) > 5 else 0,
            "field_6": self._parse_int(record[6]) if len(record) > 6 else 0,
        }
        decoded["start_offset"] = self._parse_int(decoded["start_offset_hex"])
        span_units = decoded["field_4"] or decoded["field_3"]
        decoded["estimated_span_units"] = span_units
        decoded["estimated_size_bytes"] = span_units * decoded["block_size"] if decoded["block_size"] and span_units else 0
        decoded["offset_validity"] = self._classify_offset_validity(decoded["start_offset"], range_end)
        decoded["classification"] = self._classify_record(decoded, range_end)
        decoded["score"] = self._score_record(decoded)
        return decoded

    def _parse_int(self, value: str) -> int:
        try:
            return int(value, 0)
        except (TypeError, ValueError):
            return 0

    def _classify_offset_validity(self, start_offset: int, range_end: int) -> str:
        if start_offset <= 0:
            return "invalid"
        if range_end and start_offset >= range_end:
            return "wrapped_or_out_of_range"
        return "within_scan_range"

    def _classify_record(self, decoded: dict, range_end: int) -> dict:
        offset = decoded.get("start_offset", 0)
        size_bytes = decoded.get("estimated_size_bytes", 0)
        block_size = decoded.get("block_size", 0)
        validity = decoded.get("offset_validity", "invalid")
        reasons: list[str] = []

        if validity != "within_scan_range":
            reasons.append("offset is outside scan range or looks wrapped")
            return {
                "label": "out_of_range_marker",
                "bucket": "out_of_range",
                "confidence": "medium",
                "reasons": reasons,
            }

        if offset < 1024 * 1024 and size_bytes <= 512 * 1024:
            reasons.append("small structure near beginning of scanned volume")
            return {
                "label": "boot_or_metadata_candidate",
                "bucket": "metadata_like",
                "confidence": "medium",
                "reasons": reasons,
            }

        if block_size == 4096 and size_bytes >= 100 * 1024 * 1024:
            reasons.append("large 4K-aligned span more consistent with FS/volume fragment than single media file")
            return {
                "label": "large_fs_fragment_candidate",
                "bucket": "metadata_like",
                "confidence": "medium",
                "reasons": reasons,
            }

        if block_size == 4096 and 512 * 1024 <= size_bytes <= 32 * 1024 * 1024:
            reasons.append("mid-sized 4K-aligned span could be file extent or FS fragment")
            return {
                "label": "possible_file_or_fragment",
                "bucket": "promising",
                "confidence": "low",
                "reasons": reasons,
            }

        if range_end and offset + size_bytes <= range_end and size_bytes > 0:
            reasons.append("record fits within scan range but semantics are unclear")
            return {
                "label": "unknown_in_range_record",
                "bucket": "unknown",
                "confidence": "low",
                "reasons": reasons,
            }

        return {
            "label": "unknown_record",
            "bucket": "unknown",
            "confidence": "low",
            "reasons": ["insufficient evidence"],
        }

    def _score_record(self, decoded: dict) -> int:
        classification = decoded.get("classification", {})
        bucket = classification.get("bucket", "unknown")
        label = classification.get("label", "unknown")
        size_bytes = decoded.get("estimated_size_bytes", 0)
        validity = decoded.get("offset_validity", "invalid")

        score = 0
        if validity == "within_scan_range":
            score += 20
        if bucket == "promising":
            score += 50
        elif bucket == "metadata_like":
            score += 15
        elif bucket == "unknown":
            score += 5
        elif bucket == "out_of_range":
            score -= 30

        if 1 * 1024 * 1024 <= size_bytes <= 32 * 1024 * 1024:
            score += 20
        elif 256 * 1024 * 1024 <= size_bytes:
            score -= 10
        elif 256 * 1024 <= size_bytes < 1 * 1024 * 1024:
            score += 10

        if label == "boot_or_metadata_candidate":
            score -= 15
        if label == "large_fs_fragment_candidate":
            score -= 10
        return score

