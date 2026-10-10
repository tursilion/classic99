# 3rdparty

Thirdparty libraries with source

## pico9918-core

[pico9918-core](https://github.com/visrealm/pico9918-core) is built from the VDP engine
running inside the PICO9918, by Troy Schrapel (visrealm), MIT licensed. It is vendored
here so Classic99 can offer it as a VDP engine.

| | |
| --- | --- |
| `pico9918-core/` | the sources Classic99 compiles. `UPSTREAM.txt` records the revision they came from. |
| `pico9918-core-gen/` | the headers and image arrays the library's CMake build would produce. `GENERATED.txt` records the options they were built with. |
| `pico9918_msvc_c89.h` | `_Static_assert` and `_Alignof` for a toolset without `/std:c11`. |
| `update-pico9918-core.py` | moves the library to a new upstream revision. |

Nothing here is edited by hand and changes belong upstream, because
`pico9918-core/` is replaced wholesale on the next fetch. Only the part Classic99
compiles is vendored - the tests, examples, bindings and the CMake build stay
upstream.

### Building

Nothing to do. `classic99.vcxproj` lists these sources and compiles them
alongside Classic99's own, so building Classic99 needs neither CMake nor Python.

### Moving to a new revision

Run from the repository root:

```
python 3rdparty/update-pico9918-core.py --fetch
python 3rdparty/update-pico9918-core.py --fetch 1944f45a5f945f66c57e73e624f9ca2861fa312e
python 3rdparty/update-pico9918-core.py --check
```

`--fetch` with no revision takes `main`; a named commit needs its whole
40-character SHA. `--check` writes nothing and reports whether the committed
generated files still match the sources. Python 3.8+, and git for `--fetch`.

Build, then commit `pico9918-core/` and `pico9918-core-gen/` together.

The script refuses rather than guessing - a library option it does not recognise
is the usual one, and it says what to add where.