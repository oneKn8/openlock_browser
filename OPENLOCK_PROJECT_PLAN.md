# OpenLock — Open-Source Linux Lockdown Exam Browser
## Complete Project Plan & Build Guide

---

## 1. PROJECT VISION

**OpenLock** is an open-source, Linux-native secure exam browser that provides equivalent lockdown functionality to Respondus LockDown Browser and Safe Exam Browser (SEB). It targets compatibility with major Learning Management Systems (Canvas, Moodle, Blackboard, Brightspace, Sakai, Schoology) and aims to become the standard for Linux-based exam security.

**End goal:** A browser that a university IT department can evaluate, approve, and deploy — giving Linux users equal access to proctored assessments.

**License:** MPL-2.0 (permissive enough for institutional adoption, copyleft enough to stay open)

---

## 2. ARCHITECTURE OVERVIEW

```
┌─────────────────────────────────────────────────────┐
│                   OpenLock Application               │
├──────────┬──────────┬───────────┬───────────────────┤
│  Kiosk   │ Process  │ Input     │  System           │
│  Shell   │ Guard    │ Lockdown  │  Integrity        │
│  Module  │ Module   │ Module    │  Module           │
├──────────┴──────────┴───────────┴───────────────────┤
│              Core Lockdown Engine                    │
├─────────────────────────────────────────────────────┤
│         QtWebEngine (Chromium-based browser)         │
├──────────┬──────────────────────────────────────────┤
│  SEB     │  LMS Direct Integration                  │
│  Protocol│  (Canvas, Moodle, Blackboard, etc.)      │
│  Module  │                                          │
├──────────┴──────────────────────────────────────────┤
│         Linux Platform Layer                         │
│    (X11 / Wayland / Cage compositor / cgroups)       │
└─────────────────────────────────────────────────────┘
```

### Core Components

| Component | Responsibility | Technology |
|---|---|---|
| **Kiosk Shell** | Fullscreen takeover, prevent desktop access | Cage (Wayland compositor) or X11 override-redirect |
| **Process Guard** | Kill/block prohibited processes, monitor for new ones | procfs monitoring, cgroups v2, Linux namespaces |
| **Input Lockdown** | Grab keyboard, block shortcuts, disable clipboard | XGrabKeyboard / libinput grab / Wayland protocols |
| **System Integrity** | VM detection, debugger detection, integrity self-check | CPUID, DMI tables, ptrace detection, binary signing |
| **Browser Engine** | Render LMS pages, handle quiz interactions | QtWebEngine (Chromium) |
| **SEB Protocol Module** | Parse .seb config files, generate Browser Exam Keys | SEB open protocol implementation |
| **LMS Integration Layer** | Handle LMS-specific handshakes and redirects | Per-LMS adapters |

### Tech Stack

- **Language:** C++17 (primary), Python (tooling/tests)
- **GUI Framework:** Qt6
- **Browser Engine:** QtWebEngine (Chromium-based — same foundation as Respondus itself)
- **Build System:** CMake
- **Packaging:** AppImage (universal), .deb, .rpm, Flatpak
- **CI/CD:** GitHub Actions
- **Testing:** GoogleTest (unit), Selenium (browser integration), custom lockdown verification suite

---

## 3. BUILD PHASES

### PHASE 0 — Project Scaffolding (Week 1)

**Goal:** Repo structure, build system, CI pipeline, empty shells for all modules.

#### Directory Structure
```
openlock/
├── CMakeLists.txt
├── LICENSE.txt                    # MPL-2.0
├── README.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── SECURITY_MODEL.md          # Critical for university adoption
│   ├── LMS_COMPATIBILITY.md
│   └── CONTRIBUTING.md
├── src/
│   ├── main.cpp                   # Entry point
│   ├── core/
│   │   ├── LockdownEngine.cpp     # Orchestrates all lockdown modules
│   │   ├── LockdownEngine.h
│   │   ├── Config.cpp             # Configuration parser (.seb, .openlock)
│   │   └── Config.h
│   ├── kiosk/
│   │   ├── KioskShell.cpp         # Fullscreen kiosk management
│   │   ├── KioskShell.h
│   │   ├── X11Kiosk.cpp           # X11-specific implementation
│   │   ├── WaylandKiosk.cpp       # Wayland-specific (Cage compositor)
│   │   └── PlatformKiosk.h        # Abstract interface
│   ├── guard/
│   │   ├── ProcessGuard.cpp       # Process monitoring and blocking
│   │   ├── ProcessGuard.h
│   │   ├── ProcessBlocklist.cpp   # Maintains list of prohibited processes
│   │   └── CGroupIsolator.cpp     # cgroup-based resource isolation
│   ├── input/
│   │   ├── InputLockdown.cpp      # Keyboard/mouse grab
│   │   ├── InputLockdown.h
│   │   ├── ClipboardGuard.cpp     # Clipboard monitoring and clearing
│   │   ├── ShortcutBlocker.cpp    # Block Alt+Tab, PrtSc, Ctrl+C, etc.
│   │   └── PrintBlocker.cpp       # Disable CUPS / print dialogs
│   ├── integrity/
│   │   ├── SystemIntegrity.cpp    # VM detection, debugger detection
│   │   ├── SystemIntegrity.h
│   │   ├── VMDetector.cpp         # Virtual machine detection
│   │   ├── DebugDetector.cpp      # Anti-debugging / anti-tampering
│   │   └── SelfVerifier.cpp       # Binary integrity self-check
│   ├── browser/
│   │   ├── SecureBrowser.cpp      # QtWebEngine wrapper with restrictions
│   │   ├── SecureBrowser.h
│   │   ├── NavigationFilter.cpp   # URL whitelist/blacklist enforcement
│   │   ├── DownloadBlocker.cpp    # Prevent file downloads during exam
│   │   └── DevToolsBlocker.cpp    # Block Chromium DevTools access
│   ├── protocol/
│   │   ├── SEBProtocol.cpp        # Safe Exam Browser protocol handler
│   │   ├── SEBProtocol.h
│   │   ├── SEBConfigParser.cpp    # Parse .seb config files
│   │   ├── BrowserExamKey.cpp     # Generate SEB Browser Exam Keys
│   │   └── ConfigKeyGenerator.cpp # Generate SEB Config Keys
│   └── lms/
│       ├── LMSAdapter.h           # Abstract LMS interface
│       ├── MoodleAdapter.cpp      # Moodle-specific integration
│       ├── CanvasAdapter.cpp      # Canvas-specific integration
│       └── BlackboardAdapter.cpp  # Blackboard-specific integration
├── tests/
│   ├── unit/
│   │   ├── test_process_guard.cpp
│   │   ├── test_vm_detector.cpp
│   │   ├── test_seb_config.cpp
│   │   └── test_navigation_filter.cpp
│   ├── integration/
│   │   ├── test_lockdown_full.py  # Full lockdown verification
│   │   └── test_lms_handshake.py  # LMS connectivity tests
│   └── security/
│       ├── test_escape_attempts.py # Attempt to break out of lockdown
│       └── test_bypass_vectors.py  # Known bypass techniques
├── config/
│   ├── default.openlock            # Default configuration
│   ├── blocklist.json              # Default process blocklist
│   └── sample.seb                  # Sample SEB config for testing
├── packaging/
│   ├── appimage/
│   ├── deb/
│   └── rpm/
└── scripts/
    ├── build.sh
    ├── install-deps.sh
    └── verify-lockdown.sh          # Script to verify lockdown is working
```

#### Tasks
- [ ] Initialize git repo with this structure
- [ ] Write CMakeLists.txt with all targets and dependencies
- [ ] Set up GitHub Actions CI for Ubuntu 22.04, 24.04, Fedora 39+
- [ ] Create stub files for every module with interfaces defined
- [ ] Write README.md with project vision, build instructions, contributing guide
- [ ] Set up pre-commit hooks (clang-format, clang-tidy)

---

### PHASE 1 — Kiosk Shell + Basic Browser (Weeks 2–3)

**Goal:** Launch a fullscreen Chromium-based browser that takes over the screen and can navigate to an LMS login page.

#### 1.1 Kiosk Shell
- Implement X11 fullscreen override-redirect window
- Implement Wayland kiosk using Cage compositor as subprocess
- Auto-detect display server (X11 vs Wayland) and select backend
- Disable window decorations, taskbar, dock, system tray
- Cover all monitors (multi-monitor lockdown)
- Handle display hotplug (prevent plugging in a second monitor to escape)

#### 1.2 Embedded Browser
- Initialize QtWebEngine with hardened settings:
  - Disable JavaScript console / DevTools
  - Disable file:// protocol access
  - Disable downloads
  - Disable right-click context menu (or restrict it)
  - Disable page source viewing
  - Set custom User-Agent string identifying OpenLock
  - Disable WebRTC (prevents screen sharing to other devices)
  - Disable extensions
- Create minimal toolbar: Back, Forward, Refresh, Stop only
- Navigate to a configurable start URL (institution LMS login page)

#### 1.3 Navigation Filter
- Implement URL whitelist system (only LMS domains + explicitly allowed URLs)
- Block navigation to any non-whitelisted domain
- Handle redirects — allow auth redirects (SSO/CAS/Shibboleth) while still filtering
- Block `about:`, `chrome://`, `file://`, `data:` URL schemes
- Allow instructor-configured exceptions (for open-book resources)

#### Deliverable: A fullscreen browser that opens, navigates to an LMS, and cannot be minimized or closed except via a controlled exit.

---

### PHASE 2 — Process Guard + Input Lockdown (Weeks 4–5)

**Goal:** Lock down the rest of the system so nothing outside the browser is accessible.

#### 2.1 Process Guard
- On startup, scan running processes against a blocklist:
  - Screen capture: `obs`, `ffmpeg`, `recordmydesktop`, `simplescreenrecorder`, `kazam`, `peek`, `vlc` (recording mode), `wf-recorder`
  - Screen sharing: `zoom`, `teams`, `discord`, `slack`, `skype`, `anydesk`, `teamviewer`, `rustdesk`
  - Messaging: `telegram`, `signal`, `pidgin`, `thunderbird`
  - Virtual machines: `virtualbox`, `vmware`, `qemu`, `virt-manager`
  - Remote desktop: `xrdp`, `vino`, `remmina`, `x11vnc`, `tigervnc`
  - Terminals: `gnome-terminal`, `konsole`, `xterm`, `alacritty`, `kitty`, `tmux`, `screen`
  - Browsers: `firefox`, `chromium`, `brave`, `vivaldi`, `opera` (any non-OpenLock browser)
  - Automation: `xdotool`, `xautomation`, `ydotool`, `wtype`
- Prompt user to close blocked processes before exam starts
- Continuously monitor `/proc` for new process launches during exam
- Kill newly launched prohibited processes immediately (with logging)
- Use Linux cgroups v2 to isolate OpenLock — prevent new process creation outside the cgroup
- Maintain configurable blocklist (JSON) — institutions can customize

#### 2.2 Input Lockdown
- Grab all keyboard input:
  - X11: `XGrabKeyboard()` on root window
  - Wayland: Cage compositor handles this natively (only OpenLock gets input)
- Block dangerous key combinations:
  - `Alt+Tab`, `Alt+F4`, `Ctrl+Alt+Delete`, `Super/Meta` key
  - `PrtSc`, `Shift+PrtSc` (screenshots)
  - `Ctrl+Alt+F1–F12` (VT switching)
  - `Ctrl+Alt+Backspace` (X11 kill)
  - `Ctrl+Shift+I`, `F12` (DevTools)
  - `Ctrl+U` (view source)
  - `Ctrl+S` (save page)
  - `Ctrl+P` (print)
  - `Ctrl+Shift+J` (console)
- Allow: `Ctrl+C/V` only within the browser's text input fields if instructor enables it
- Clipboard Guard:
  - Clear X11 PRIMARY, SECONDARY, CLIPBOARD selections on exam start
  - Monitor and clear clipboard at regular intervals (every 500ms)
  - Block clipboard content originating from outside the browser window
- Print Blocker:
  - Stop CUPS service temporarily or block access to CUPS socket
  - Intercept print dialog requests in QtWebEngine
  - Re-enable CUPS on controlled exit

#### Deliverable: System is fully locked down — no app switching, no screenshots, no copy from outside, no printing.

---

### PHASE 3 — System Integrity (Week 6)

**Goal:** Detect cheating environments (VMs, debuggers, tampering).

#### 3.1 VM Detection
Check multiple signals (any single check can be spoofed; combine them):
- `systemd-detect-virt` — returns hypervisor name or "none"
- CPUID hypervisor bit (ECX bit 31 of CPUID leaf 1)
- DMI/SMBIOS strings in `/sys/class/dmi/id/` — look for "VirtualBox", "VMware", "QEMU", "Xen", "KVM", "Hyper-V", "Parallels"
- Check `/proc/scsi/scsi` for virtual disk identifiers
- MAC address OUI prefixes (08:00:27 = VirtualBox, 00:0C:29 = VMware, 52:54:00 = QEMU)
- Check for hypervisor-specific kernel modules (`vboxguest`, `vmw_balloon`, `virtio`)
- Timing-based detection (VM exits cause measurable latency)
- Check `/proc/cpuinfo` for hypervisor flag

**Policy:** Make VM detection configurable. Some institutions allow VMs (for accessibility), others don't. Default = block VMs, but let config override.

#### 3.2 Debugger / Tampering Detection
- Check `ptrace` status via `/proc/self/status` (TracerPid)
- Attempt `ptrace(PTRACE_TRACEME)` on self — fails if already being traced
- Monitor for `strace`, `ltrace`, `gdb`, `lldb` in process list
- Check if running under `LD_PRELOAD` (library injection)
- Verify binary integrity: compute SHA-256 of own executable and compare against known-good hash
- Check for `/proc/self/maps` anomalies (injected shared libraries)

#### 3.3 Self-Verification
- Compute and report a **Browser Exam Key** (hash of binary + configuration)
- This key can be checked server-side by the LMS to verify an unmodified client
- Implement signed configuration files (institution signs configs with their key)

#### Deliverable: OpenLock detects and refuses to run in compromised environments, and can prove its own integrity to the server.

---

### PHASE 4 — SEB Protocol Compatibility (Weeks 7–9)

**Goal:** Full compatibility with Safe Exam Browser protocol so OpenLock works with any LMS that supports SEB — especially Moodle (which has native SEB support).

**THIS IS THE MOST CRITICAL PHASE FOR ADOPTION.** SEB is open-source and its protocol is documented. If OpenLock speaks SEB, it works with Moodle out of the box. This is your golden ticket.

#### 4.1 Understanding SEB Protocol

SEB's security model is based on two keys that the client sends to the server:

**Browser Exam Key (BEK):**
- SHA-256 hash of: `SEB_binary_hash + SEB_config_hash + URL_of_current_page`
- Sent as an HTTP header: `X-SafeExamBrowser-RequestHash`
- Server compares this against expected value to verify the client is genuine and unmodified
- The BEK changes per-page, so the server can verify on every request

**Config Key (CK):**
- SHA-256 hash of the SEB configuration file contents
- Sent as an HTTP header: `X-SafeExamBrowser-ConfigKeyHash`
- Verifies the configuration hasn't been tampered with

#### 4.2 Implementation Steps

1. **Parse .seb config files**
   - SEB configs are XML plist format (Apple-style property lists), optionally compressed and/or encrypted
   - Parse all relevant settings: allowed URLs, blocked processes, enable/disable specific features
   - Support both plaintext and encrypted .seb files (AES-256-CBC, password-derived key)

2. **Generate Browser Exam Key**
   - Compute SHA-256 of the OpenLock binary
   - Compute SHA-256 of the loaded configuration
   - For each HTTP request, compute: `SHA256(binary_hash + config_hash + request_url)`
   - Inject this as `X-SafeExamBrowser-RequestHash` header in every request

3. **Generate Config Key**
   - Compute SHA-256 of the raw config data
   - Inject as `X-SafeExamBrowser-ConfigKeyHash` header

4. **Handle SEB launch links**
   - Register as handler for `seb://` and `sebs://` URL schemes on Linux (via .desktop file and xdg-mime)
   - When a `seb://` link is clicked in a regular browser, OpenLock launches and loads the config

5. **Communicate with SEB Server (optional)**
   - SEB has an optional server component for real-time monitoring
   - Implement the SEB Server API client for: handshake, ping/heartbeat, event reporting
   - GitHub: https://github.com/SafeExamBrowser/seb-server

#### 4.3 How to Get the Server-Side Hash

**For Moodle (easiest path):**
- Moodle has built-in SEB support under: Site Administration → Plugins → Quiz → Safe Exam Browser
- Admin enters the expected Browser Exam Key hash when configuring the quiz
- To get YOUR key accepted:
  1. Build OpenLock with SEB protocol support
  2. Run OpenLock and navigate to any page → it will compute and log the BEK
  3. Give this BEK to the quiz admin (your instructor or IT)
  4. They enter it in Moodle's SEB settings for the quiz
  5. When OpenLock accesses the quiz, Moodle verifies the hash matches → access granted
- Alternatively, Moodle can be configured to "just check if SEB headers are present" without strict hash verification — this is the easiest entry point

**For Canvas / Blackboard / Other LMS:**
- These use Respondus's proprietary integration, not SEB
- However, many also support SEB via plugins or LTI integrations
- Canvas has a "Respondus LockDown Browser" integration but IT can also install the SEB Moodle-style plugin via LTI
- Fallback: OpenLock identifies itself via User-Agent string that the LMS can whitelist

**For Respondus Protocol (aspirational, Phase 6+):**
- Respondus uses a proprietary, closed protocol
- The client fetches "server profiles" from `server-profiles-respondus-com.s3-external-1.amazonaws.com`
- These profiles contain the institution's LMS URL and settings
- The verification protocol is NOT publicly documented
- Reverse-engineering it is legally risky (DMCA/CFAA concerns)
- **Better strategy:** Don't clone Respondus protocol. Instead, build something so good that institutions switch their LMS config from "Require Respondus" to "Require SEB or OpenLock"

#### 4.4 Reference Implementation
- SEB Windows source: https://github.com/SafeExamBrowser/seb-win-refactoring (C#)
- SEB macOS source: https://github.com/SafeExamBrowser/seb-mac (Objective-C)
- SEB Server: https://github.com/SafeExamBrowser/seb-server (Java/Spring)
- SEB config documentation: https://safeexambrowser.org/developer/seb-config-key.html

Study the Windows and macOS implementations to understand exactly how BEK and CK are computed. The algorithm must match byte-for-byte or the server will reject you.

#### Deliverable: OpenLock can open .seb config files, generate valid Browser Exam Keys, and successfully access SEB-protected quizzes on Moodle.

---

### PHASE 5 — LMS-Specific Adapters (Weeks 10–11)

**Goal:** Handle quirks of each major LMS.

#### 5.1 Moodle
- Native SEB support — primary target
- Handle Moodle's quiz access rules plugin for SEB
- Support Moodle's SEB config download flow
- Test with Moodle 4.x

#### 5.2 Canvas
- Canvas checks for LockDown Browser via a Respondus-specific LTI integration
- Canvas also supports SEB via the "Quiz LTI" or "New Quizzes" with external tool config
- Implement Canvas LTI handshake if institution configures it
- Fallback: User-Agent based detection

#### 5.3 Blackboard
- Similar to Canvas — Respondus integration is proprietary
- Target SEB compatibility path via LTI

#### 5.4 SSO / Authentication Handling
- Many universities use SSO (Shibboleth, CAS, SAML, Azure AD, Google OAuth)
- OpenLock must handle multi-step auth redirects without breaking the navigation filter
- Whitelist common SSO domains: `*.cas.*`, `*.shibboleth.*`, `login.microsoftonline.com`, `accounts.google.com`
- Allow institution-specific SSO domain configuration

#### Deliverable: OpenLock works end-to-end with at least Moodle (SEB mode) and can handle SSO logins.

---

### PHASE 6 — Respondus Monitor Equivalent (Weeks 12–14, Optional)

**Goal:** Webcam proctoring support for institutions that require it.

- Access webcam via Video4Linux2 (V4L2) or GStreamer
- Record video during exam session
- Implement startup sequence: webcam check, photo capture, ID verification, environment scan
- Store recordings locally, upload to institution's server post-exam
- This is separate from the lockdown browser and should be a pluggable module

**Note:** This phase is optional and can be deferred. Many quizzes only need the lockdown browser, not webcam proctoring.

---

## 4. SERVER INTEGRATION — DETAILED BREAKDOWN

### 4.1 How the Client-Server Handshake Works

```
Student clicks "Start Quiz" in regular browser
        │
        ▼
LMS checks quiz settings → "Requires Secure Browser"
        │
        ▼
LMS redirects to seb:// or sebs:// link
(or shows "please open in LockDown Browser" page)
        │
        ▼
OpenLock launches (registered as seb:// handler)
        │
        ▼
OpenLock loads .seb config (from URL or local file)
        │
        ▼
OpenLock computes Browser Exam Key (BEK) and Config Key (CK)
        │
        ▼
OpenLock navigates to LMS quiz page with headers:
  X-SafeExamBrowser-RequestHash: <BEK>
  X-SafeExamBrowser-ConfigKeyHash: <CK>
  User-Agent: SEB/... (identifies as SEB-compatible)
        │
        ▼
LMS server validates:
  1. BEK matches expected hash → client is genuine
  2. CK matches expected config → config not tampered
  3. User-Agent contains SEB identifier
        │
        ▼
Quiz page loads inside OpenLock
        │
        ▼
Every subsequent page request includes updated BEK
(hash includes the URL, so it changes per page)
        │
        ▼
Student submits quiz → OpenLock releases lockdown
```

### 4.2 Getting Access to Respondus Servers (Honest Assessment)

**You cannot and should not try to impersonate a Respondus client to Respondus servers.**

Here's why:
- Respondus server profiles are fetched from AWS S3 buckets tied to institutional licenses
- The client-server protocol is proprietary and obfuscated
- Impersonating a Respondus client likely violates their Terms of Service
- Reverse-engineering their protocol could trigger DMCA/CFAA issues
- Even if it worked today, Respondus could change the protocol at any update and break you

**The correct strategy is not to connect to Respondus servers at all.** Instead:

1. **Target SEB protocol** (open, documented, works with Moodle natively)
2. **Convince institutions to accept OpenLock alongside Respondus** by proving equivalent security
3. **Long-term:** Get Respondus themselves to certify OpenLock (they might, if it genuinely secures the exam — it expands their TAM to Linux)

### 4.3 Moodle SEB Integration — Step by Step (Your Fastest Win)

1. Set up a local Moodle instance for testing (Docker: `docker run -p 8080:8080 bitnami/moodle`)
2. Enable SEB plugin: Site Admin → Plugins → Activity modules → Quiz → Safe Exam Browser
3. Create a test quiz, set access rule to "Require Safe Exam Browser"
4. Configure it to accept specific Browser Exam Key
5. Build OpenLock with SEB headers
6. Access the quiz → Moodle validates → quiz loads
7. Document the entire flow with screenshots
8. Present this to your university IT team: "Here's an open-source, auditable secure browser that works with your existing Moodle SEB plugin. No new server software needed."

---

## 5. WHAT CAN GO WRONG — RISK REGISTRY

### 5.1 Technical Risks

| Risk | Severity | Likelihood | Mitigation |
|---|---|---|---|
| **Browser Exam Key hash doesn't match SEB reference implementation** | CRITICAL | HIGH | Study SEB source code byte-by-byte. The hash algorithm must be IDENTICAL. Off-by-one in URL normalization, config parsing, or binary hashing will cause rejection. Set up automated comparison tests against SEB's own output. |
| **QtWebEngine behaves differently from Chromium standalone** | HIGH | MEDIUM | QtWebEngine is Chromium but with Qt's layer on top. Some JavaScript APIs may differ. Test extensively with real LMS quiz pages. Fall back to CEF (Chromium Embedded Framework) if QtWebEngine has deal-breaking issues. |
| **Wayland vs X11 fragmentation** | HIGH | HIGH | Wayland adoption is growing but X11 isn't dead. Must support both. The Cage compositor approach handles Wayland elegantly but adds a dependency. X11 grab approach is simpler but less secure. Ship both backends, auto-detect. |
| **Keyboard grab fails or is incomplete** | HIGH | MEDIUM | X11 keyboard grabs can be stolen by other apps. Wayland is better (compositor controls input). On X11, also disable VT switching via `ioctl` on `/dev/tty`. Test every known escape key combination. |
| **New screen recording tools bypass process blocklist** | MEDIUM | HIGH | Process blocklist is a cat-and-mouse game. Supplement with: monitoring `/proc` for any process accessing framebuffer or X11 SHM, and using cgroups to prevent new process creation entirely. |
| **Qt6/QtWebEngine dependency hell across distros** | MEDIUM | HIGH | Different distros ship different Qt versions. Solution: ship as AppImage with bundled Qt, or use Flatpak. Also maintain traditional .deb/.rpm for distros with compatible Qt6. |
| **SSL/TLS certificate issues with university SSO** | MEDIUM | MEDIUM | Universities use various CA chains. Bundle Mozilla's CA bundle. Allow institution-specific CA cert configuration. Handle certificate pinning for LMS domains. |
| **Multi-monitor lockdown fails** | MEDIUM | MEDIUM | Users could drag content to a second monitor or view notes there. Solution: detect all connected monitors via xrandr/wlr-randr, cover all with black overlay windows, or disable secondary outputs entirely. Monitor for hotplug events. |
| **Flatpak/AppImage sandboxing conflicts with lockdown** | MEDIUM | LOW | Flatpak sandboxing may prevent some lockdown operations (accessing /proc, cgroups). AppImage is better for this use case. If using Flatpak, need appropriate permissions. |
| **Race condition between process guard and fast-launching apps** | LOW | MEDIUM | A user could script a process to launch and screenshot in milliseconds before the guard catches it. Mitigation: use cgroup-based prevention (no new processes can spawn outside the cgroup) rather than reactive killing. |

### 5.2 Protocol / Integration Risks

| Risk | Severity | Likelihood | Mitigation |
|---|---|---|---|
| **SEB protocol changes in a new version** | HIGH | LOW | SEB is versioned and backward-compatible. Pin to a specific SEB protocol version. Monitor SEB GitHub for changes. |
| **.seb config encryption scheme is undocumented or changes** | MEDIUM | LOW | Current scheme (AES-256-CBC with PBKDF2 key derivation) is visible in SEB source code. Implement it from the source, not from guessing. |
| **Moodle SEB plugin expects specific User-Agent format** | HIGH | MEDIUM | Moodle checks for "SEB" in the User-Agent string. Must match exactly. Check Moodle source: `mod/quiz/accessrule/seb/` for the exact regex/string check. |
| **University uses Respondus exclusively, no SEB option** | CRITICAL | MEDIUM | This is the hardest scenario. Options: (a) Convince IT to enable SEB alongside Respondus, (b) Get Respondus to support Linux, (c) Show that OpenLock's lockdown is equivalent and get a policy exception. Option (a) is most realistic. |
| **LMS updates break integration** | MEDIUM | MEDIUM | LMS platforms update frequently. Set up automated integration tests against the latest Moodle, Canvas (free-for-teachers), and open-source LMS instances. Run tests in CI on a schedule. |

### 5.3 Institutional / Adoption Risks

| Risk | Severity | Likelihood | Mitigation |
|---|---|---|---|
| **IT department refuses to evaluate open-source tool** | HIGH | MEDIUM | Lead with the security whitepaper. Emphasize: open-source = auditable (unlike Respondus, which is a black box). Offer to do a supervised security assessment. Frame it as "you can verify every line of code." |
| **Professor says "just use Windows"** | HIGH | HIGH | Go above the professor. IT department → Department Chair → Dean of Students → Accessibility Office. Frame as equity issue. Document everything in writing. |
| **Respondus threatens legal action** | MEDIUM | LOW | OpenLock doesn't use any Respondus code, protocols, or trademarks. It implements the open SEB protocol. Keep clear legal separation. Do NOT reverse-engineer Respondus binaries. Use only open documentation and open-source SEB code as reference. |
| **No one contributes to the project** | MEDIUM | MEDIUM | Start by solving YOUR problem first. A working tool that one university accepts is more powerful than a half-built tool with grand ambitions. Growth comes from proven utility. |

### 5.4 Security Risks (Things That Could Compromise Exam Integrity)

| Risk | Description | Mitigation |
|---|---|---|
| **LD_PRELOAD injection** | Attacker preloads a library that intercepts lockdown functions | Check `LD_PRELOAD` and `/proc/self/maps` at startup; refuse to run if unexpected libraries are loaded |
| **ptrace attachment** | Debugger attaches to OpenLock and manipulates its behavior | Self-ptrace on startup; monitor TracerPid; refuse to run under debugger |
| **Modified binary** | User patches the OpenLock binary to disable checks | Binary self-verification (embedded hash). BEK changes if binary changes, so server rejects it anyway. |
| **Shared memory screenshot** | Tool reads X11 shared memory segments to capture screen | Run in isolated X11 display or Wayland (no SHM access for other clients) |
| **Physical second device** | Student uses phone or second laptop to look things up | Out of scope for a lockdown browser (this is what webcam proctoring addresses) |

---

## 6. TESTING STRATEGY

### 6.1 Unit Tests
- Config parser: parse valid/invalid .seb files, verify all settings extracted correctly
- BEK generation: compare output against SEB reference implementation for same inputs
- VM detection: mock various `/proc`, `/sys` outputs and verify detection
- Process blocklist: verify all known screen capture / messaging tools are caught
- Navigation filter: test allowed/blocked URLs, redirect chains, edge cases

### 6.2 Integration Tests
- Full lockdown cycle: start OpenLock → verify all lockdown measures active → exit cleanly
- Moodle integration: start quiz, answer questions, submit — end to end
- SSO flow: test with Shibboleth, CAS, Azure AD authentication chains
- Multi-monitor: connect second display, verify it's handled

### 6.3 Security Tests (Escape Attempts)
These tests INTENTIONALLY try to break out of the lockdown. They should all FAIL:
- Launch a terminal during exam
- Take a screenshot (every known tool)
- Switch to another window (Alt+Tab, workspace switch)
- Open a second browser
- Access clipboard content from before exam
- Connect via VNC/SSH from another machine
- Launch a process after exam starts
- Modify running process memory
- Switch to a TTY (Ctrl+Alt+F2)

### 6.4 Compatibility Tests
- Ubuntu 22.04, 24.04
- Fedora 39, 40
- Arch Linux (rolling)
- Linux Mint 21, 22
- Debian 12
- X11 and Wayland on each
- GNOME, KDE, XFCE desktop environments

---

## 7. UNIVERSITY ADOPTION PLAYBOOK

### Step 1: Build the Proof of Concept (You are here)
Get Phase 0–4 working. You need a demo.

### Step 2: Document Everything
- Write a **Security Model** document comparing OpenLock's protections vs Respondus feature-by-feature
- Create a **compatibility matrix** showing which LMS platforms are supported
- Record a **demo video** showing the lockdown in action (and attempted escapes failing)

### Step 3: Approach IT Department (Not Your Professor)
- Email the Director of IT or Chief Information Security Officer (CISO)
- Subject line: "Open-Source Secure Exam Browser for Linux — Request for Evaluation"
- Include: security model doc, demo video link, GitHub repo link
- Emphasize: "This is auditable, open-source software that implements the same SEB protocol your Moodle already supports"

### Step 4: Offer a Pilot
- Propose: "Let me use this for my own quizzes in [course name] as a pilot"
- Offer to take quizzes in a supervised setting (IT staff can watch) to prove it works
- Ask for a single course as a trial

### Step 5: Get Formal Approval
- After successful pilot, ask IT to write a policy memo approving OpenLock
- Get it added to the institution's "supported testing software" list
- Share the success with the Linux community

### Step 6: Publish and Publicize
- Write a blog post about the project
- Post on r/linux, r/opensource, Hacker News
- Present at a university tech talk or conference
- Contact SEB developers about potential collaboration or endorsement
- Contact Respondus — seriously. "We built an open-source lockdown browser for Linux. Can we work together on compatibility?"

---

## 8. QUICK REFERENCE — KEY URLS AND RESOURCES

| Resource | URL |
|---|---|
| SEB Windows Source | https://github.com/SafeExamBrowser/seb-win-refactoring |
| SEB macOS Source | https://github.com/SafeExamBrowser/seb-mac |
| SEB Server Source | https://github.com/SafeExamBrowser/seb-server |
| SEB Config Key Docs | https://safeexambrowser.org/developer/seb-config-key.html |
| LXEB (existing Linux effort) | https://github.com/BC100Dev/LXEB |
| Moodle SEB Plugin Source | https://github.com/moodle/moodle/tree/master/mod/quiz/accessrule/seb |
| Moodle SEB Docs | https://docs.moodle.org/en/Safe_Exam_Browser |
| QtWebEngine Docs | https://doc.qt.io/qt-6/qtwebengine-index.html |
| Cage Wayland Compositor | https://github.com/cage-kiosk/cage |
| Respondus Firewall/Network Info | https://support.respondus.com/hc/en-us/articles/29456577007003 |

---

## 9. BUILD ORDER FOR CLAUDE CODE

When dropping this into Claude Code, follow this exact sequence:

```
1. Create project scaffold (Phase 0)
   → CMakeLists.txt, directory structure, stub files

2. Implement Config system first
   → Parse .openlock configs and .seb configs
   → This drives everything else

3. Build the browser shell (Phase 1)
   → QtWebEngine window with minimal toolbar
   → Navigation filter
   → Get it opening an LMS login page

4. Add kiosk mode (Phase 1)
   → X11 fullscreen takeover
   → Multi-monitor handling

5. Add process guard (Phase 2)
   → Blocklist loading
   → /proc scanner
   → Continuous monitoring thread

6. Add input lockdown (Phase 2)
   → Keyboard grab
   → Clipboard guard
   → Shortcut blocking

7. Add integrity checks (Phase 3)
   → VM detection
   → Debugger detection
   → Self-verification

8. Implement SEB protocol (Phase 4) ← THE BIG ONE
   → .seb config parser with encryption support
   → Browser Exam Key generation
   → Config Key generation
   → HTTP header injection
   → seb:// URL scheme registration

9. Moodle integration test (Phase 5)
   → Docker Moodle instance
   → Create test quiz with SEB requirement
   → Verify OpenLock passes server validation

10. Packaging
    → AppImage build script
    → .deb packaging
    → .rpm packaging
```

---

## 10. SUCCESS METRICS

You'll know this project is working when:

- [ ] OpenLock runs fullscreen and cannot be escaped without the exit password
- [ ] All known bypass attempts fail (terminal, screenshot, Alt+Tab, VT switch)
- [ ] VM detection correctly identifies at least 5 hypervisors
- [ ] .seb config files parse correctly (validated against SEB reference output)
- [ ] Browser Exam Key matches SEB reference implementation for the same inputs
- [ ] Moodle quiz with SEB requirement loads successfully in OpenLock
- [ ] At least one university IT department agrees to evaluate it
- [ ] You pass a quiz using OpenLock on your Linux machine

---

*This document is the complete blueprint. Build it phase by phase, test relentlessly, and document everything. The Linux community is waiting for this.*
