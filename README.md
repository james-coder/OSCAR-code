Open Source CPAP Analysis Reporter (OSCAR)
==========================================

OSCAR lets patients and clinicians review CPAP, oximetry, and related sleep-therapy data across platforms. It is a community-maintained continuation of SleepyHead 1.1.0.

Highlights
----------
- Cross‑platform: Windows, macOS, and Linux builds via Qt.
- Supports a wide range of CPAP devices and oximeters.
- Daily, overview, and reporting views for therapy tracking.
- Translations for many languages and bundled help.

Quick Start
-----------
1. Install Qt (5.15.x recommended for current builds) and a C++ toolchain.
2. Build using qmake:
   - `mkdir build && cd build`
   - `qmake ../OSCAR_QT.pro`
   - `make` (or `mingw32-make` on Windows)
3. Run the built application from `build/oscar/OSCAR` (or `OSCAR.exe` / `OSCAR.app`).

Platform Build Guides
---------------------
- **Linux:** `Building/Linux/BUILD_Linux.md`
- **macOS:** `Building/MacOS/BUILD-mac.md`
- **Windows:** `Building/Windows/BUILD-WIN.md`

Contributing
------------
We welcome issues, pull requests, translations, and testing on all supported platforms.

Support
-------
- Release notes and docs: `oscar/docs`
- Translations: `Translations/`

License and Attribution
-----------------------
- OSCAR is released under the GNU GPL v3. See `COPYING`.
- Portions © 2019‑2025 The OSCAR Team; derived from SleepyHead © 2011‑2018 Mark Watkins.

Redistribution Note
-------------------
If you resell derivatives of OSCAR or SleepyHead, clearly credit that your product is based on the free and open-source software SleepyHead, developed and copyrighted by Mark Watkins (2011‑2018). Simply saying “based on GPL software” is not sufficient. See GPL terms in `COPYING`.
