#!/usr/bin/env python3

"""
Build script for GitHub Actions.

Runs on Windows, planned to support Linux and Mac as well.
Calls CMake.

Receives input via environment variables set via the matrix,
not command line arguments.
"""

import argparse
import os
import re
import shlex
import shutil
import subprocess
import sys
import typing as T
from contextlib import contextmanager
from pathlib import Path


@contextmanager
def pushd(new_dir: T.Union[Path, str]) -> T.Iterator[None]:
    previous_dir = os.getcwd()
    os.chdir(str(new_dir))
    try:
        yield
    finally:
        os.chdir(previous_dir)


def sanitize_path(filename: str) -> str:
    """Replace all characters except alphanumerics and separators with underscores."""
    return re.sub(r"[^a-zA-Z0-9_\-.]", r"_", filename)


def quote_str(path: Path) -> str:
    return shlex.quote(str(path))


def run(*strings: T.List[str], **kwargs):
    # fmt: off
    args = [arg
        for s in strings
        for arg in shlex.split(s)
    ]
    # fmt: on
    subprocess.run(args, check=True, **kwargs)


def parse_bool_int(s: str) -> T.Optional[bool]:
    return {"": False, "0": False, "1": True}.get(s, None)


"""Debug or release build."""
CONFIGURATION = os.environ.get("CONFIGURATION", "Release")

"""Version string."""
APPVEYOR_BUILD_VERSION = os.environ.get("APPVEYOR_BUILD_VERSION", "UnknownVer")


def ARCHIVE():
    """Output archive file name."""

    configuration = "" if CONFIGURATION == "Release" else f"-{CONFIGURATION}"

    # TODO indicate 32/64 and compiler/OS
    return f"qvgmsplit-v{APPVEYOR_BUILD_VERSION}{configuration}-dev"


ARCHIVE = ARCHIVE()


def resolve_compilers():
    """CMake expects CC and CXX environment variables to be absolute compiler paths."""
    for compiler in ["CC", "CXX"]:
        path = os.environ[compiler]
        os.environ[compiler] = shutil.which(path)


# BUILD_DIR = sanitize_path(f"build-{APPVEYOR_JOB_NAME}-{CONFIGURATION}")
BUILD_DIR = "build"


def build():
    CMAKE_USER_BEGIN = Path("cmake_user_begin.cmake").resolve()

    resolve_compilers()

    if DISABLE_PCH:
        with CMAKE_USER_BEGIN.open("a") as f:
            f.write("set(USE_PCH FALSE)\n")

    os.makedirs(BUILD_DIR, exist_ok=True)
    os.chdir(BUILD_DIR)

    run(f"cmake .. -DCMAKE_BUILD_TYPE={CONFIGURATION} -G Ninja")
    run("ninja")


ARCHIVE_ROOT = "archive-root"
EXE_NAME = "qvgmsplit"


def archive():
    root_dir = Path().resolve()
    build_dir = Path(BUILD_DIR).resolve()
    archive_name = os.path.abspath(ARCHIVE)

    shutil.rmtree(ARCHIVE_ROOT, ignore_errors=True)
    os.mkdir(ARCHIVE_ROOT)

    def copy_to_cwd(in_file: str):
        """Copies a file to the current directory without renaming it.
        Copying instead of renaming makes archive() idempotent
        and simplifies local testing."""
        # TODO switch to two arguments
        shutil.copy(str(build_dir / in_file), Path(in_file).name)

    with pushd(ARCHIVE_ROOT):
        if sys.platform == "win32":
            copy_to_cwd(f"{EXE_NAME}.exe")
        elif sys.platform.startswith("linux"):
            copy_to_cwd(EXE_NAME)
        else:
            raise Exception(f"unknown OS {sys.platform}, cannot determine binary name")

        # Archive Qt DLLs.
        run(
            "windeployqt.exe",
            f"{EXE_NAME}.exe",
            # "--verbose 2",
            # Reduce file size.
            "--no-compiler-runtime --no-svg --no-angle --no-opengl-sw",
        )

        # Remove unnecessary Qt code.
        shutil.rmtree("imageformats")

        # I'm not sure where it comes from. I assume CMake copies it
        # from vcpkg to the build directory at some point. Anyway let's
        # use it.
        if CONFIGURATION == "Debug":
            copy_to_cwd("zlibd1.dll")
        else:
            copy_to_cwd("zlib1.dll")


class DefaultHelpParser(argparse.ArgumentParser):
    def error(self, message):
        sys.stderr.write("error: %s\n" % message)
        self.print_help(sys.stderr)
        sys.exit(2)


def main():
    # create the top-level parser
    parser = DefaultHelpParser()

    def f():
        subparsers = parser.add_subparsers(dest="cmd")
        subparsers.required = True

        parser_build = subparsers.add_parser("build")
        parser_archive = subparsers.add_parser("archive")

    f()
    args = parser.parse_args()

    if args.cmd == "build":
        # Not currently used.
        build()

    if args.cmd == "archive":
        archive()


if __name__ == "__main__":
    main()
