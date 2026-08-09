<p align="center">
  <img src="https://img.shields.io/badge/Platform-Linux-informational?style=flat&logo=linux&logoColor=white&color=FCC624" alt="Linux"/>
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-blue?style=flat&logo=cplusplus&logoColor=white" alt="C++17"/>
  <img src="https://img.shields.io/badge/Framework-Qt6-41CD52?style=flat&logo=qt&logoColor=white" alt="Qt6"/>
  <img src="https://img.shields.io/badge/Engine-Chromium-4285F4?style=flat&logo=googlechrome&logoColor=white" alt="Chromium"/>
  <img src="https://img.shields.io/badge/License-MPL--2.0-orange?style=flat" alt="MPL-2.0"/>
</p>

# OpenLock

**Open-source lockdown exam browser for Linux.**

OpenLock is a secure, kiosk-mode browser that brings the lockdown guarantees of Respondus LockDown Browser and Safe Exam Browser (SEB) to Linux, built natively rather than through a compatibility layer. It lets students on Ubuntu, Fedora, Arch, and other distributions take proctored exams without dual-booting into Windows or macOS.

---

## Why OpenLock?

Most universities require Respondus or SEB for proctored exams. Neither supports Linux. Students running Linux are forced to dual-boot, use a VM (which gets detected and blocked), or borrow a different machine.

OpenLock targets this by implementing the **SEB open protocol** directly: it parses `.seb` configs and attaches **Browser Exam Key** and **Config Key** headers to every request, so an LMS can recognise the session the way it recognises an official SEB client.

> **Status: not exam-ready.** The lockdown layer is implemented and unit tested. The SEB key
> derivation is **not yet byte-exact against the spec**, so the headers OpenLock sends will not
> match what an LMS computes, and the handshake will fail. It has never been validated against a
> live Moodle, Canvas or Blackboard instance. Do not use this for a real exam. See
> [Current limitations](#current-limitations).

---

## Features

| Category | What it does |
|---|---|
| **SEB Protocol** | Parses `.seb` config files (RNCryptor v3), attaches BEK and Config Key headers per request (derivation not yet spec-exact, see limitations) |
| **Kiosk Mode** | Fullscreen takeover via X11 override-redirect or Wayland/Cage compositor |
| **Process Guard** | Monitors and kills prohibited processes (screen recorders, VMs, remote desktop) |
| **Input Lockdown** | Blocks Alt+Tab, Alt+F4, Ctrl+C, PrintScreen, clipboard, and other escape shortcuts |
| **Browser Hardening** | Disables DevTools, View Source, downloads, right-click, console, drag-and-drop |
| **System Integrity** | Detects VMs (CPUID, DMI, MAC), debuggers (ptrace), hypervisors |
| **Navigation Control** | Whitelist/blacklist URL filtering, back/forward policy enforcement |
| **LMS Support** | Adapters for Moodle, Canvas, Blackboard with LMS-specific handshake handling |

---

## Architecture

```
+-----------------------------------------------------+
|                   OpenLock Application               |
+----------+----------+-----------+-------------------+
|  Kiosk   | Process  |  Input    |  System           |
|  Shell   | Guard    |  Lockdown |  Integrity        |
+----------+----------+-----------+-------------------+
|              Core Lockdown Engine                    |
+-----------------------------------------------------+
|         QtWebEngine (Chromium-based browser)         |
+----------+------------------------------------------+
|  SEB     |  LMS Integration                         |
|  Protocol|  (Canvas, Moodle, Blackboard)             |
+----------+------------------------------------------+
|         Linux Platform Layer                         |
|    (X11 / Wayland / cgroups / procfs)                |
+-----------------------------------------------------+
```

---

## Quick Start

### Prerequisites

- Ubuntu 22.04+ / Fedora 38+ / Arch Linux
- Qt6 with QtWebEngine
- CMake 3.22+
- g++ 11+ or clang++ 14+
- OpenSSL 3.x

### Install dependencies (Ubuntu/Debian)

```bash
sudo ./scripts/install-deps.sh
```

Or manually:

```bash
sudo apt install build-essential cmake \
  qt6-base-dev qt6-webengine-dev libqt6webenginecore6-bin \
  libssl-dev libx11-dev libxtst-dev libxcb-xkb-dev \
  libxkbcommon-dev pkg-config
```

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Run tests

```bash
cd build && ctest --output-on-failure
```

### Launch

```bash
./build/openlock                          # Opens default start page
./build/openlock --seb config.seb         # Opens a .seb config file
./build/openlock --url https://exam.url   # Opens a specific exam URL
```

---

## Project Structure

```
openlock/
  CMakeLists.txt
  src/
    main.cpp
    core/           Config, LockdownEngine
    browser/        SecureBrowser, NavigationFilter, DevToolsBlocker, DownloadBlocker
    protocol/       SEBConfigParser, BrowserExamKey, ConfigKeyGenerator, SEBRequestInterceptor
    guard/          ProcessGuard, ProcessBlocklist, CGroupIsolator
    input/          InputLockdown, ShortcutBlocker, ClipboardGuard, PrintBlocker
    integrity/      VMDetector, DebugDetector, SelfVerifier, SystemIntegrity
    kiosk/          KioskShell, PlatformKiosk, X11Kiosk, WaylandKiosk
    lms/            MoodleAdapter, CanvasAdapter, BlackboardAdapter
  tests/
    unit/           GoogleTest-based unit tests
  config/           Default config, blocklists, sample .seb file
  scripts/          Build helpers, dependency installer, lockdown verifier
  packaging/        (planned) AppImage, .deb, .rpm, Flatpak
```

---

## SEB Compatibility

OpenLock targets the SEB open protocol. What is wired end to end:

- **.seb file parsing:** gzip + RNCryptor v3 decryption (password and certificate modes)
- **Header injection:** `QWebEngineUrlRequestInterceptor` attaches SEB headers to every outgoing request
- **Browser Exam Key (BEK):** HMAC-SHA256 over config plus binary hash, sent as `X-SafeExamBrowser-RequestHash`
- **Config Key:** SHA256, sent as `X-SafeExamBrowser-ConfigKeyHash`

### Current limitations

Two gaps mean the keys are computed but **not yet correct**, so no LMS will accept the handshake:

1. **Config Key is not built from the parsed settings.** The spec requires SHA256 over a canonical
   SEB-JSON string: keys sorted case-insensitively, `originatorVersion` removed, no whitespace.
   `ConfigKeyGenerator` implements exactly that, but it is never handed the parsed settings map, so
   it falls back to hashing the raw config bytes instead.
2. **`examKeySalt` is not extracted from the `.seb` config.** It falls back to a hash of the raw
   config data, so the BEK does not match either.

Neither is validated by a test against known-good SEB output, which is why this went unnoticed.
Closing both, plus adding real SEB test vectors, is what stands between this and a usable release.

No live LMS testing has been done against Moodle, Canvas or Blackboard.

---

## Security Model

OpenLock applies defense in depth:

1. **Process isolation** -- cgroups v2 restrict child processes; prohibited apps are killed on detection
2. **Input capture** -- X11 keyboard grab or Wayland input lockdown prevents escape sequences
3. **Browser sandbox** -- Off-the-record profile, no persistent storage, all permissions denied by default
4. **Integrity checks** -- VM/debugger/hypervisor detection at startup; optional self-hash verification
5. **Navigation control** -- URL whitelist/blacklist filtering; back/forward/refresh policies from .seb config

---

## Roadmap

- [x] Core lockdown engine
- [x] SEB protocol scaffolding (.seb parser, header injection, key generators)
- [ ] SEB keys byte-exact against spec (settings map wiring + examKeySalt extraction)
- [ ] SEB interop test vectors from a reference client
- [x] Process guard with blocklist
- [x] Input lockdown (X11)
- [x] Browser hardening
- [x] Unit tests (4 suites)
- [ ] End-to-end Moodle exam testing
- [ ] Wayland/Cage kiosk mode
- [ ] AppImage packaging
- [ ] .deb / .rpm packaging
- [ ] CI/CD pipeline (GitHub Actions)
- [ ] Proctoring camera integration
- [ ] Accessibility support

---

## Contributing

Contributions are welcome. Please open an issue first to discuss what you'd like to change.

This project follows the [MPL-2.0](LICENSE.txt) license -- you can use OpenLock in proprietary products as long as modifications to MPL-licensed files remain open.

---

## License

[Mozilla Public License 2.0](LICENSE.txt)
