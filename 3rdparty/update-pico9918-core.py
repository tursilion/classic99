#!/usr/bin/env python3
#
# Refresh the vendored pico9918-core build artifacts.
#
# (C) 2026 Troy Schrapel (visrealm)
#
# pico9918-core's sources live in this tree - not a submodule, not a second build
# system. This script is what puts them there and keeps them honest: it fetches a
# revision from upstream, copies in the part Classic99 compiles, and generates the
# build-config header, version header and four PNG-derived C arrays that the library's
# own CMake would otherwise produce.
#
#   python 3rdparty/update-pico9918-core.py            regenerate, no network
#   python 3rdparty/update-pico9918-core.py --fetch     replace the sources with main
#   python 3rdparty/update-pico9918-core.py --fetch 1944f45
#   python 3rdparty/update-pico9918-core.py --source ../pico9918-core
#   python 3rdparty/update-pico9918-core.py --check
#
# Needs Python 3.8+, and git for --fetch. Nothing else.
#

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

DEFAULT_SOURCE = os.path.join(HERE, "pico9918-core")
DEFAULT_OUT = os.path.join(HERE, "pico9918-core-gen")
VCXPROJ = os.path.join(ROOT, "classic99.vcxproj")

UPSTREAM = "https://github.com/visrealm/pico9918-core.git"
DEFAULT_REVISION = "main"
STAMP = "UPSTREAM.txt"

# What lands in the tree. The library, the licence it is under, the two documents that
# are its integration contract, and the converter that turns its PNGs into C arrays.
# Not the tests, examples or bindings: Classic99 compiles none of them and they are
# four times the size of what it does.
VENDOR = [
    "LICENSE",
    "README.md",
    "BUILDING.md",
    "EMULATOR-INTEGRATION.md",
    "src",
    "tools/img2carray.py",
]

# NOT vendored, however much of src/ is: a CMakeLists in the tree is a second build
# system to everyone who opens the folder, whatever a readme says. What this script
# reads from them - the version and the library's source list - is taken at fetch and
# stamped, so the checks they back still run.
VENDOR_EXCLUDED = {"CMakeLists.txt"}

# Changing one means rerunning this script, not just rebuilding: the generated
# build-config header records them and the library rejects a consumer that disagrees.
OPTIONS = {
    "PICO9918_TEXT80_8BPP": True,
    "PICO9918_RUNTIME_CHIP": True,
    "PICO9918_DEBUG_API": True,
    "PICO9918_LAYER_MASK": True,
    "PICO9918_STEP_CALLBACK": True,
    "PICO9918_SINGLE_INSTANCE": False,
    "PICO9918_NO_SPLASH": False,
}

# Not 4, the desktop default - the committed image assets are built for this.
ASSET_PIXEL_SIZE = 2

CORE_SOURCES = [
    "pico9918.c",
    "pico9918_util.c",
    "pico9918_config.c",
    "pico9918_palette.c",
    "pico9918_frame.c",
    "overlay/splash.c",
    "overlay/diag.c",
    "gpu/gpu.c",
    "gpu/tms9900.c",
]

if OPTIONS["PICO9918_DEBUG_API"]:
    CORE_SOURCES.append("pico9918_debug.c")

# Left out on purpose, so a source the library gains is an error below.
CORE_SOURCES_EXCLUDED = {
    "gpu/v9938cmd.c",
}


def image_assets():
    """Output base name, PNG under src/overlay/, and the array symbol to force."""
    assets = [("bmp_font", "res/font.png", None)]

    if not OPTIONS["PICO9918_NO_SPLASH"]:
        assets.append(("bmp_splash", "res/splash.png", "splash"))

    if OPTIONS["PICO9918_RUNTIME_CHIP"]:
        # not gated on NO_SPLASH - an F18A still shows its badge
        assets.append(("bmp_f18a_badge", "res/f18a_badge.png", "f18aBadge"))
        if not OPTIONS["PICO9918_NO_SPLASH"]:
            assets.append(("bmp_splash_pro", "res/splash_pro.png", "splashPro"))

    return assets


class Failure(Exception):
    pass


def read(path):
    with open(path, "r", encoding="utf-8") as f:
        return f.read()


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    # CRLF explicitly, not the platform's, or the committed files churn
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)


def check_version(major, minor, patch):
    # 1.16 packs exactly as 1.0 does, so this overflow is otherwise silent
    if major > 15 or minor > 15 or patch > 255:
        raise Failure(
            "pico9918-core {}.{}.{} does not fit the stored identity: "
            "PICO9918_CONF_SW_VERSION is one nibble each for major and minor, "
            "PICO9918_CONF_SW_PATCH_VERSION one byte. Widening them is a "
            "config-block ABI migration, not a version bump."
            .format(major, minor, patch))

    return major, minor, patch


def cmake_version(tree):
    path = os.path.join(tree, "CMakeLists.txt")
    matched = re.search(
        r"project\(pico9918_core\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", read(path))

    if not matched:
        raise Failure(
            "could not read VERSION from {}\n"
            "The config block carries a version stamp and pico9918_config_validate() "
            "uses it to decide whether stored settings need migrating, so an empty "
            "one cannot be built around.".format(path))

    return check_version(*(int(g) for g in matched.groups()))


def core_version(source):
    recorded = stamped(source, "version")
    if recorded:
        matched = re.match(r"(\d+)\.(\d+)\.(\d+)$", recorded)
        if not matched:
            raise Failure("{} records an unreadable version: {}"
                          .format(os.path.join(source, STAMP), recorded))
        return check_version(*(int(g) for g in matched.groups()))

    return cmake_version(source)


def generate_version_header(out, version):
    major, minor, patch = version

    write(os.path.join(out, "pico9918_core_version.h"),
          "/* GENERATED by 3rdparty/update-pico9918-core.py - do not edit. */\n"
          "#ifndef PICO9918_CORE_VERSION_H\n"
          "#define PICO9918_CORE_VERSION_H\n"
          "\n"
          "#define PICO9918_CORE_VER_MAJOR {}\n"
          "#define PICO9918_CORE_VER_MINOR {}\n"
          "#define PICO9918_CORE_VER_PATCH {}\n"
          "#define PICO9918_CORE_VER_STRING \"{}.{}.{}\"\n"
          "\n"
          "#endif\n"
          .format(major, minor, patch, major, minor, patch))


def generate_build_config(source, out, version):
    major, minor, patch = version

    values = {
        "PICO9918_ASSET_PIXEL_SIZE": str(ASSET_PIXEL_SIZE),
        "PICO9918_BUILD_TEXT80_8BPP": "1" if OPTIONS["PICO9918_TEXT80_8BPP"] else "0",
        "PICO9918_BUILD_RUNTIME_CHIP": "1" if OPTIONS["PICO9918_RUNTIME_CHIP"] else "0",
        "PICO9918_BUILD_DEBUG_API": "1" if OPTIONS["PICO9918_DEBUG_API"] else "0",
        "PICO9918_BUILD_LAYER_MASK": "1" if OPTIONS["PICO9918_LAYER_MASK"] else "0",
        "PICO9918_BUILD_STEP_CALLBACK":
            "1" if OPTIONS["PICO9918_STEP_CALLBACK"] else "0",
        "PICO9918_BUILD_SINGLE_INSTANCE":
            "1" if OPTIONS["PICO9918_SINGLE_INSTANCE"] else "0",
        "PICO9918_BUILD_SW_VERSION": "0x{:x}".format((major << 4) | minor),
        "PICO9918_BUILD_SW_PATCH": str(patch),
    }

    template = os.path.join(source, "src", "pico9918_build_config.h.in")
    text = read(template)

    # Filling in what we know and leaving the rest would defeat the header silently.
    unknown = sorted(set(re.findall(r"@(\w+)@", text)) - set(values))
    if unknown:
        raise Failure(
            "{} has placeholders this script does not know how to fill:\n"
            "  {}\n"
            "pico9918-core has gained a build choice. Add it to OPTIONS and to "
            "values in generate_build_config(), then regenerate."
            .format(os.path.relpath(template, ROOT), ", ".join(unknown)))

    for name, value in values.items():
        text = text.replace("@{}@".format(name), value)

    text = text.replace(
        "DO NOT EDIT - generated from pico9918_build_config.h.in by CMake.",
        "DO NOT EDIT - generated from pico9918_build_config.h.in by\n"
        " * 3rdparty/update-pico9918-core.py.")

    write(os.path.join(out, "pico9918_build_config.h"), text)


def generate_image_assets(source, out):
    tool = os.path.join(source, "tools", "img2carray.py")
    if not os.path.exists(tool):
        raise Failure("{} is missing - is --source really a pico9918-core tree?"
                      .format(os.path.relpath(tool, ROOT)))

    overlay = os.path.join(source, "src", "overlay")

    for name, png, symbol in image_assets():
        src_png = os.path.join(overlay, png)
        if not os.path.exists(src_png):
            raise Failure("overlay image {} is missing".format(src_png))

        command = [sys.executable, tool, "-r", src_png]
        if symbol:
            command += ["-s", symbol]
        command += ["-o", os.path.join(out, "overlay", name + ".c")]

        os.makedirs(os.path.join(out, "overlay"), exist_ok=True)
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise Failure("img2carray.py failed on {}:\n{}{}"
                          .format(png, result.stdout, result.stderr))

        # The tool records the input path it was handed verbatim, which would
        # commit an absolute path off whoever ran this.
        for produced in (name + ".c", name + ".h"):
            path = os.path.join(out, "overlay", produced)
            text = read(path)
            for spelling in (src_png, src_png.replace("/", os.sep),
                             src_png.replace(os.sep, "/")):
                text = text.replace(spelling, "src/overlay/" + png)
            write(path, text)


def check_library_sources(tree):
    """What the library builds, against what this script says it does."""
    cmakelists = os.path.join(tree, "src", "CMakeLists.txt")
    text = read(cmakelists)

    offered = set()
    for matched in re.finditer(
            r"(?:set|list\(\s*APPEND)\s*\(?\s*PICO9918_SOURCES\b", text):
        depth, idx = 0, matched.start()
        while idx < len(text):
            if text[idx] == "(":
                depth += 1
            elif text[idx] == ")":
                depth -= 1
                if depth == 0:
                    break
            idx += 1
        offered.update(re.findall(r"[\w/]+\.c\b", text[matched.start():idx]))

    unaccounted = offered - set(CORE_SOURCES) - CORE_SOURCES_EXCLUDED
    if unaccounted:
        raise Failure(
            "pico9918-core offers sources this script does not account for:\n"
            "  {}\n"
            "Decide whether Classic99 builds them, then add each to CORE_SOURCES "
            "or to CORE_SOURCES_EXCLUDED with the reason."
            .format(", ".join(sorted(unaccounted))))

    missing = set(CORE_SOURCES) - offered
    if missing:
        raise Failure(
            "these are in CORE_SOURCES but {} no longer builds them:\n"
            "  {}\n"
            "The library has dropped or renamed them - update CORE_SOURCES and "
            "classic99.vcxproj together."
            .format(os.path.relpath(cmakelists, ROOT), ", ".join(sorted(missing))))


def check_project_sources():
    """What Classic99 compiles, against what this script says it does."""
    project = read(VCXPROJ)
    listed = set(re.findall(
        r'ClCompile Include="3rdparty\\pico9918-core\\src\\([^"]+)"', project))
    listed = {path.replace("\\", "/") for path in listed}

    if listed != set(CORE_SOURCES):
        raise Failure(
            "classic99.vcxproj does not compile what this script expects.\n"
            "  only in the project: {}\n"
            "  only in the script:  {}"
            .format(", ".join(sorted(listed - set(CORE_SOURCES))) or "-",
                    ", ".join(sorted(set(CORE_SOURCES) - listed)) or "-"))

    # The generated overlay headers guard the pixel policy with #ifdef, so a
    # project that does not define it drops the check rather than failing.
    declared = re.search(r"PICO9918_ASSET_PIXEL_SIZE=(\d+)", project)
    if not declared or int(declared.group(1)) != ASSET_PIXEL_SIZE:
        raise Failure(
            "classic99.vcxproj must define PICO9918_ASSET_PIXEL_SIZE={}, and says {}."
            .format(ASSET_PIXEL_SIZE,
                    declared.group(1) if declared else "nothing"))


def force_remove(func, path, _):
    # git leaves its object store read-only, and rmtree honours that on Windows
    os.chmod(path, 0o700)
    func(path)


def git(where, *args, check=True):
    try:
        result = subprocess.run(["git", "-C", where] + list(args),
                                capture_output=True, text=True)
    except OSError:
        raise Failure("git is not on PATH, and --fetch needs it")

    if check and result.returncode != 0:
        raise Failure("git {} failed:\n{}".format(" ".join(args),
                                                  result.stderr.strip() or result.stdout.strip()))
    return result


def fetch(revision, into):
    print("fetching {} from {}".format(revision, UPSTREAM))

    git(into, "init", "--quiet")
    git(into, "remote", "add", "origin", UPSTREAM)

    # A revision, not a branch: --depth 1 --branch takes only the latter, and the point
    # of naming one is to be able to pin a SHA the same way.
    fetched = git(into, "fetch", "--quiet", "--depth", "1", "origin", revision, check=False)
    if fetched.returncode != 0:
        raise Failure(
            "upstream has no {}\n"
            "A tag or branch by name, or a commit by its WHOLE 40-character SHA - "
            "GitHub will not serve an abbreviated one.\n{}"
            .format(revision, fetched.stderr.strip()))

    git(into, "checkout", "--quiet", "FETCH_HEAD")

    return git(into, "rev-parse", "HEAD").stdout.strip()


def vendor(fetched, source, revision):
    # against the fetched tree, which still has the CMakeLists: this is the only moment
    # the library's own source list can have changed, and the last one it is readable
    version = cmake_version(fetched)
    check_library_sources(fetched)

    if os.path.exists(source):
        shutil.rmtree(source)

    for entry in VENDOR:
        src = os.path.join(fetched, entry.replace("/", os.sep))
        dst = os.path.join(source, entry.replace("/", os.sep))

        if not os.path.exists(src):
            raise Failure(
                "pico9918-core {} has no {}\n"
                "The library has moved something this integration vendors. Fix VENDOR "
                "in this script, and check whether the build needs it too."
                .format(revision[:7], entry))

        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if os.path.isdir(src):
            shutil.copytree(src, dst,
                            ignore=lambda where, names: VENDOR_EXCLUDED.intersection(names))
        elif os.path.basename(src) not in VENDOR_EXCLUDED:
            shutil.copy2(src, dst)

    write(os.path.join(source, STAMP), "\n".join([
        "pico9918-core, vendored into Classic99",
        "",
        "VENDORED by 3rdparty/update-pico9918-core.py - do not edit here.",
        "Changes belong upstream; this tree is replaced wholesale on the next fetch.",
        "",
        "  origin    {}".format(UPSTREAM),
        "  revision  {}".format(revision),
        "  version   {}.{}.{}".format(*version),
        "",
        "Only the part Classic99 compiles. The tests, examples and bindings are",
        "upstream, and so is the CMake build - this tree is built by classic99.vcxproj",
        "and nothing else.",
        "",
    ]))


def stamped(source, field):
    path = os.path.join(source, STAMP)
    if not os.path.exists(path):
        return None

    for line in read(path).splitlines():
        if line.strip().startswith(field + " "):
            return line.split(None, 1)[1].strip()
    return None


def upstream_revision(source):
    recorded = stamped(source, "revision")
    if recorded:
        return recorded

    # --source pointed at somebody's own checkout. TRAP: rev-parse in a directory that
    # is not itself a repository answers for the ENCLOSING one, so without this the
    # stamp would carry Classic99's own HEAD as though it were the library's.
    top = git(source, "rev-parse", "--show-toplevel", check=False)
    if top.returncode != 0:
        return "unknown"
    if os.path.normcase(os.path.abspath(top.stdout.strip())) != os.path.normcase(source):
        return "unknown"

    revision = git(source, "rev-parse", "HEAD", check=False).stdout.strip() or "unknown"

    # HEAD alone would name a revision these files were NOT generated from whenever the
    # tree has been edited in place, which is how a core change gets tried out before it
    # is pushed.
    dirty = git(source, "status", "--porcelain", check=False)
    if dirty.returncode == 0 and dirty.stdout.strip():
        revision += "-dirty"

    return revision


def generate_stamp(source, out, version):
    # No timestamp, or --check cannot be believed
    lines = [
        "pico9918-core vendored build artifacts",
        "",
        "GENERATED by 3rdparty/update-pico9918-core.py - do not edit.",
        "",
        "  version   {}.{}.{}".format(*version),
        "  revision  {}".format(upstream_revision(source)),
        "",
        "Built with:",
    ]
    for name, value in sorted(OPTIONS.items()):
        lines.append("  {:<28} {}".format(name, "ON" if value else "OFF"))
    lines.append("  {:<28} {}".format("PICO9918_ASSET_PIXEL_SIZE", ASSET_PIXEL_SIZE))
    lines += [
        "",
        "These are the files pico9918-core's CMake build would generate. Classic99",
        "builds under Visual Studio alone, so they are generated once and committed.",
        "Rerun the script after moving the library, and commit the result with it.",
        "",
    ]
    write(os.path.join(out, "GENERATED.txt"), "\n".join(lines))


def generate_all(source, out, version):
    generate_version_header(out, version)
    generate_build_config(source, out, version)
    generate_image_assets(source, out)
    generate_stamp(source, out, version)


def same_content(left, right):
    # endings normalised, so a checkout with core.autocrlf off is not called stale
    with open(left, "rb") as f:
        a = f.read().replace(b"\r\n", b"\n")
    with open(right, "rb") as f:
        b = f.read().replace(b"\r\n", b"\n")
    return a == b


def differences(left, right):
    """Paths under `right` that `left` does not match."""
    out = []
    for base, _, names in os.walk(left):
        for name in names:
            produced = os.path.join(base, name)
            relative = os.path.relpath(produced, left)
            existing = os.path.join(right, relative)
            if not os.path.exists(existing):
                out.append(relative + " (missing)")
            elif not same_content(produced, existing):
                out.append(relative + " (stale)")

    for base, _, names in os.walk(right):
        for name in names:
            relative = os.path.relpath(os.path.join(base, name), right)
            if not os.path.exists(os.path.join(left, relative)):
                out.append(relative + " (no longer generated)")

    return sorted(out)


def main():
    parser = argparse.ArgumentParser(
        description="Refresh the vendored pico9918-core build artifacts.")
    parser.add_argument(
        "--source", default=DEFAULT_SOURCE, metavar="DIR",
        help="pico9918-core checkout to generate from (default: 3rdparty/pico9918-core)")
    parser.add_argument(
        "--out", default=DEFAULT_OUT, metavar="DIR",
        help="where the generated files go (default: 3rdparty/pico9918-core-gen)")
    parser.add_argument(
        "--check", action="store_true",
        help="report whether the committed files are up to date, write nothing")
    parser.add_argument(
        "--fetch", nargs="?", const=DEFAULT_REVISION, metavar="REV",
        help="replace the vendored sources with REV from upstream (default: {})"
             .format(DEFAULT_REVISION))
    args = parser.parse_args()

    source = os.path.abspath(args.source)
    out = os.path.abspath(args.out)

    try:
        if args.fetch and args.check:
            raise Failure("--check reports on what is committed, so it cannot --fetch")

        if args.fetch:
            if source != os.path.abspath(DEFAULT_SOURCE):
                raise Failure("--fetch replaces the vendored tree, so it cannot take --source")

            staging = tempfile.mkdtemp(prefix="pico9918-core-")
            try:
                revision = fetch(args.fetch, staging)
                vendor(staging, source, revision)
            finally:
                shutil.rmtree(staging, onerror=force_remove)

        if not os.path.exists(os.path.join(source, "src", "pico9918.c")):
            raise Failure(
                "no pico9918-core at {}\n"
                "Fetch it, or point --source at a checkout:\n"
                "  python 3rdparty/update-pico9918-core.py --fetch".format(source))

        version = core_version(source)
        check_project_sources()

        # a checkout rather than the vendored tree, so its CMakeLists is there to check
        if not stamped(source, "revision"):
            check_library_sources(source)

        if args.check:
            staging = tempfile.mkdtemp(prefix="pico9918-core-gen-")
            try:
                generate_all(source, staging, version)
                stale = differences(staging, out)
            finally:
                shutil.rmtree(staging, ignore_errors=True)

            if stale:
                print("pico9918-core generated files are out of date:")
                for entry in stale:
                    print("  " + entry)
                print("\nRun: python 3rdparty/update-pico9918-core.py")
                return 1

            print("pico9918-core {}.{}.{}: generated files are up to date"
                  .format(*version))
            return 0

        # cleared, so an asset the library stops generating goes away
        if os.path.exists(out):
            shutil.rmtree(out)

        generate_all(source, out, version)

        print("pico9918-core {}.{}.{} -> {}"
              .format(*version, os.path.relpath(out, ROOT)))
        for base, _, names in sorted(os.walk(out)):
            for name in sorted(names):
                print("  " + os.path.relpath(
                    os.path.join(base, name), out).replace("\\", "/"))
        return 0

    except Failure as failure:
        sys.stderr.write("\nupdate-pico9918-core: {}\n\n".format(failure))
        return 1


if __name__ == "__main__":
    sys.exit(main())
