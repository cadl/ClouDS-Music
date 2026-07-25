#!/usr/bin/env python3
"""Build deterministic binary and complete-source release archives."""

from __future__ import annotations

import argparse
import datetime as dt
import gzip
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile


PROJECT_DOCS = (
    "README.md",
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "TRADEMARKS.md",
    "SECURITY.md",
    "CONTRIBUTING.md",
    "CODE_OF_CONDUCT.md",
    "docs/HARDWARE_TEST.md",
    "docs/AI_SUPPORT_FAQ.md",
)
BINARY_FILES = (
    "ClouDS-Music.3dsx",
    "ClouDS-Music.smdh",
    "ClouDS-Music.cia",
)
RIME_SOURCE_FILES = (
    "8105.dict.yaml",
    "41448.dict.yaml",
    "others.dict.yaml",
    "base.dict.yaml",
    "ext.dict.yaml",
    "tencent.dict.yaml",
)
SAFE_VERSION = re.compile(r"^[0-9A-Za-z][0-9A-Za-z._-]*$")


class PackageError(RuntimeError):
    pass


def git_output(project_root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=project_root,
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip()


def require_regular_file(path: Path) -> None:
    if path.is_symlink() or not path.is_file():
        raise PackageError(f"required regular file is missing: {path}")


def normalized_mode(path: Path) -> int:
    return 0o755 if path.stat().st_mode & 0o111 else 0o644


def validate_archive_name(name: str) -> str:
    pure = PurePosixPath(name)
    if pure.is_absolute() or ".." in pure.parts or not pure.parts:
        raise PackageError(f"unsafe archive path: {name}")
    return pure.as_posix()


def tracked_files(
    project_root: Path, include_untracked: bool
) -> list[tuple[str, Path, int]]:
    outputs = [subprocess.run(
        ["git", "ls-files", "-z"],
        cwd=project_root,
        check=True,
        stdout=subprocess.PIPE,
    ).stdout]
    if include_untracked:
        outputs.append(
            subprocess.run(
                ["git", "ls-files", "-z", "--others", "--exclude-standard"],
                cwd=project_root,
                check=True,
                stdout=subprocess.PIPE,
            ).stdout
        )
    entries: list[tuple[str, Path, int]] = []
    for raw_name in b"\0".join(outputs).split(b"\0"):
        if not raw_name:
            continue
        name = validate_archive_name(raw_name.decode("utf-8"))
        path = project_root / name
        # A gitlink is listed by the superproject but is a directory on disk.
        if path.is_dir() and name == "external/minimp3":
            continue
        require_regular_file(path)
        entries.append((name, path, normalized_mode(path)))
    return entries


def submodule_files(project_root: Path) -> list[tuple[str, Path, int]]:
    submodule_root = project_root / "external/minimp3"
    require_regular_file(submodule_root / "minimp3_ex.h")
    output = subprocess.run(
        ["git", "-C", str(submodule_root), "ls-files", "-z"],
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    entries: list[tuple[str, Path, int]] = []
    for raw_name in output.split(b"\0"):
        if not raw_name:
            continue
        relative = validate_archive_name(raw_name.decode("utf-8"))
        path = submodule_root / relative
        require_regular_file(path)
        name = validate_archive_name(f"external/minimp3/{relative}")
        entries.append((name, path, normalized_mode(path)))
    return entries


def read_rime_commit(project_root: Path) -> str:
    script = (project_root / "tools/fetch-ime-assets.sh").read_text(
        encoding="utf-8"
    )
    match = re.search(r"^COMMIT=([0-9a-f]{40})$", script, re.MULTILINE)
    if match is None:
        raise PackageError("cannot resolve the pinned rime-ice commit")
    return match.group(1)


def source_entries(
    project_root: Path, epoch: int, include_untracked: bool
) -> list[tuple[str, bytes, int]]:
    entries: list[tuple[str, bytes, int]] = []
    for name, path, mode in tracked_files(
        project_root, include_untracked
    ) + submodule_files(project_root):
        entries.append((name, path.read_bytes(), mode))

    source_root = project_root / "data/pinyin_dict_src"
    for filename in RIME_SOURCE_FILES:
        path = source_root / filename
        require_regular_file(path)
        entries.append(
            (f"data/pinyin_dict_src/{filename}", path.read_bytes(), 0o644)
        )

    repository_commit = git_output(project_root, "rev-parse", "HEAD")
    submodule_commit = git_output(
        project_root / "external/minimp3", "rev-parse", "HEAD"
    )
    rime_commit = read_rime_commit(project_root)
    timestamp = dt.datetime.fromtimestamp(epoch, dt.timezone.utc).isoformat()
    provenance = (
        "ClouDS Music complete source archive\n"
        f"repository commit: {repository_commit}\n"
        f"external/minimp3 commit: {submodule_commit}\n"
        f"rime-ice commit: {rime_commit}\n"
        f"SOURCE_DATE_EPOCH: {epoch} ({timestamp})\n"
        "Input SHA-256 values are recorded in tools/fetch-ime-assets.sh.\n"
    ).encode("utf-8")
    entries.append(("SOURCE_PROVENANCE.txt", provenance, 0o644))
    return unique_entries(entries)


def binary_entries(project_root: Path) -> list[tuple[str, bytes, int]]:
    entries: list[tuple[str, bytes, int]] = []
    for name in BINARY_FILES + PROJECT_DOCS:
        path = project_root / name
        require_regular_file(path)
        entries.append((name, path.read_bytes(), normalized_mode(path)))

    for path in sorted((project_root / "third_party").rglob("*")):
        if path.is_dir():
            continue
        name = validate_archive_name(path.relative_to(project_root).as_posix())
        require_regular_file(path)
        entries.append((name, path.read_bytes(), normalized_mode(path)))

    minimp3_license = project_root / "external/minimp3/LICENSE"
    require_regular_file(minimp3_license)
    entries.append(
        (
            "third_party/minimp3-CC0-1.0.txt",
            minimp3_license.read_bytes(),
            0o644,
        )
    )
    return unique_entries(entries)


def unique_entries(
    entries: list[tuple[str, bytes, int]],
) -> list[tuple[str, bytes, int]]:
    by_name: dict[str, tuple[bytes, int]] = {}
    for name, data, mode in entries:
        safe_name = validate_archive_name(name)
        if safe_name in by_name:
            raise PackageError(f"duplicate archive path: {safe_name}")
        by_name[safe_name] = (data, mode)
    return [(name, *by_name[name]) for name in sorted(by_name)]


def zip_timestamp(epoch: int) -> tuple[int, int, int, int, int, int]:
    value = dt.datetime.fromtimestamp(epoch, dt.timezone.utc)
    if value.year < 1980:
        value = dt.datetime(1980, 1, 1, tzinfo=dt.timezone.utc)
    # ZIP timestamps have two-second resolution.
    return (value.year, value.month, value.day, value.hour, value.minute,
            value.second - value.second % 2)


def write_zip(
    output: Path,
    prefix: str,
    entries: list[tuple[str, bytes, int]],
    epoch: int,
) -> None:
    timestamp = zip_timestamp(epoch)
    with zipfile.ZipFile(
        output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for name, data, mode in entries:
            archive_name = validate_archive_name(f"{prefix}/{name}")
            info = zipfile.ZipInfo(archive_name, timestamp)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (mode & 0xFFFF) << 16
            archive.writestr(info, data)


def write_tar_gz(
    output: Path,
    prefix: str,
    entries: list[tuple[str, bytes, int]],
    epoch: int,
) -> None:
    tar_buffer = io.BytesIO()
    with tarfile.open(
        fileobj=tar_buffer, mode="w", format=tarfile.PAX_FORMAT
    ) as archive:
        for name, data, mode in entries:
            archive_name = validate_archive_name(f"{prefix}/{name}")
            info = tarfile.TarInfo(archive_name)
            info.size = len(data)
            info.mtime = epoch
            info.mode = mode
            info.uid = 0
            info.gid = 0
            info.uname = "root"
            info.gname = "root"
            archive.addfile(info, io.BytesIO(data))
    tar_buffer.seek(0)
    with output.open("wb") as raw_output:
        with gzip.GzipFile(
            filename="", mode="wb", compresslevel=9, fileobj=raw_output,
            mtime=epoch
        ) as compressed:
            shutil.copyfileobj(tar_buffer, compressed)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def copy_binary_assets(project_root: Path, output_dir: Path) -> list[Path]:
    assets: list[Path] = []
    for name in BINARY_FILES:
        source = project_root / name
        destination = output_dir / name
        require_regular_file(source)
        shutil.copyfile(source, destination)
        require_regular_file(destination)
        if sha256(source) != sha256(destination):
            raise PackageError(f"copied release asset does not match: {name}")
        assets.append(destination)
    return assets


def verify_archives(binary_zip: Path, source_tar: Path, version: str) -> None:
    binary_required = {
        f"ClouDS-Music-{version}/LICENSE",
        f"ClouDS-Music-{version}/THIRD_PARTY_NOTICES.md",
        f"ClouDS-Music-{version}/ClouDS-Music.3dsx",
        f"ClouDS-Music-{version}/ClouDS-Music.cia",
        f"ClouDS-Music-{version}/third_party/minimp3-CC0-1.0.txt",
    }
    with zipfile.ZipFile(binary_zip) as archive:
        names = set(archive.namelist())
    missing = binary_required - names
    if missing:
        raise PackageError(f"binary archive is incomplete: {sorted(missing)}")

    source_required = {
        f"ClouDS-Music-{version}-source/external/minimp3/minimp3_ex.h",
        f"ClouDS-Music-{version}-source/data/pinyin_dict_src/base.dict.yaml",
        f"ClouDS-Music-{version}-source/SOURCE_PROVENANCE.txt",
    }
    with tarfile.open(source_tar, "r:gz") as archive:
        names = set(archive.getnames())
    missing = source_required - names
    if missing:
        raise PackageError(f"source archive is incomplete: {sorted(missing)}")


def verify_output_directory(output_dir: Path, expected_names: set[str]) -> None:
    unexpected = sorted(
        path.name for path in output_dir.iterdir()
        if path.name not in expected_names
    )
    if unexpected:
        raise PackageError(
            "release output directory contains unexpected files; "
            f"remove them before packaging: {unexpected}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--project-root", type=Path, default=Path.cwd())
    parser.add_argument("--output-dir", type=Path, default=Path("dist"))
    parser.add_argument(
        "--allow-dirty",
        action="store_true",
        help="package the current worktree, including non-ignored untracked files",
    )
    args = parser.parse_args()

    if not SAFE_VERSION.fullmatch(args.version):
        raise PackageError(f"unsafe version: {args.version}")
    project_root = args.project_root.resolve()
    output_dir = (
        args.output_dir
        if args.output_dir.is_absolute()
        else project_root / args.output_dir
    ).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    binary_name = f"ClouDS-Music-{args.version}.zip"
    source_name = f"ClouDS-Music-{args.version}-source.tar.gz"
    checksum_name = "SHA256SUMS"
    verify_output_directory(
        output_dir,
        {binary_name, source_name, checksum_name, *BINARY_FILES},
    )

    worktree_status = git_output(
        project_root, "status", "--porcelain", "--untracked-files=all"
    )
    if worktree_status and not args.allow_dirty:
        raise PackageError(
            "refusing to package a dirty worktree; commit the release state first"
        )

    epoch_text = os.environ.get("SOURCE_DATE_EPOCH")
    epoch = (
        int(epoch_text)
        if epoch_text is not None
        else int(git_output(project_root, "show", "-s", "--format=%ct", "HEAD"))
    )
    if epoch < 0:
        raise PackageError("SOURCE_DATE_EPOCH must be non-negative")

    binary_zip = output_dir / binary_name
    source_tar = output_dir / source_name
    write_zip(
        binary_zip,
        f"ClouDS-Music-{args.version}",
        binary_entries(project_root),
        epoch,
    )
    write_tar_gz(
        source_tar,
        f"ClouDS-Music-{args.version}-source",
        source_entries(project_root, epoch, args.allow_dirty),
        epoch,
    )
    binary_assets = copy_binary_assets(project_root, output_dir)

    verify_archives(binary_zip, source_tar, args.version)
    checksums = output_dir / checksum_name
    checksum_text = "".join(
        f"{sha256(path)}  {path.name}\n"
        for path in sorted(
            (binary_zip, source_tar, *binary_assets),
            key=lambda item: item.name,
        )
    )
    with checksums.open("w", encoding="utf-8", newline="\n") as output:
        output.write(checksum_text)
    print(f"created release files in {output_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, PackageError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1) from None
