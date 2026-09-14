# Amtari Roadmap

Amtari is developed incrementally. A milestone is not considered runtime-qualified merely because its interfaces or skeletons exist.

## M0 — Project foundation ✅

- Define project scope and compatibility philosophy.
- Document native, hybrid and full-compatibility execution modes.
- Establish repository/build/test skeleton.
- Define initial subsystem boundaries.
- Record supported and future Atari machine families.
- Add basic CI suitable for host-side checks.

**Qualified:** GitHub Actions host checks pass.

## M1 — 68k execution model and trap core

- [x] Define Atari guest CPU state.
- [x] Define bounded Atari guest address-space access helpers.
- [x] Define safe native-68k execution boundary at the API level; native execution must never imply unchecked host pointer access.
- [x] GEMDOS TRAP #1 dispatcher skeleton.
- [x] BIOS TRAP #13 dispatcher skeleton.
- [x] XBIOS TRAP #14 dispatcher skeleton.
- [x] Host-independent unit tests for trap decoding, guest state and big-endian guest memory reads.
- [ ] CI qualification of the complete M1 host test suite.

**Exit criterion:** all M0/M1 host tests pass in CI and the trap/address-space interfaces form a stable base for implementing the first GEMDOS calls.

## M2 — Minimal TOS/GEM application environment

- GEMDOS subset for console/file/process primitives.
- Initial path/filesystem translation.
- Minimal BIOS/XBIOS services required by test programs.
- Establish legal ROM/TOS handling policy: Amtari does not redistribute proprietary ROM images.

## M3 — GEM desktop application compatibility

- VDI abstraction.
- AES abstraction.
- Keyboard/mouse translation.
- Display backend suitable for AmigaOS.
- Begin compatibility corpus of redistributable/open test programs.

## M4 — Atari ST hybrid hardware layer

- ST memory map and hardware register dispatch.
- Shifter video model.
- MFP 68901 model.
- ACIA/IKBD model.
- YM2149 sound model.
- WD1772/floppy image layer.

## M5 — ST/STE compatibility expansion

- STE video/palette extensions.
- DMA sound.
- Blitter where required.
- Improved timing and hardware-sensitive compatibility.
- ST/MSA image workflows.

## M6 — 68030-era Atari systems

- TT-oriented execution profile.
- 68030/MMU/FPU design boundaries.
- Extended video and machine-specific services.
- Preserve native execution where host CPU capabilities allow it; fall back cleanly where they do not.

## M7 — Atari Falcon

**Falcon is an explicit project target, not merely a possible future experiment.**

- Falcon 68030 machine profile.
- Falcon memory map and system-control devices.
- VIDEL video subsystem.
- Falcon DMA/audio subsystem.
- Motorola 56001 DSP abstraction/emulation backend.
- Falcon-specific XBIOS and hardware interfaces.
- Define native/hybrid boundaries for software mixing 68k and DSP workloads.
- Compatibility qualification with redistributable Falcon software.

The Falcon DSP is architecturally separate from the 68k host advantage: Amtari may execute suitable 68k guest code natively while emulating or otherwise providing the DSP subsystem.

## M8 — Full compatibility and optimization

- Compatibility fallback for software unsuitable for native execution.
- Timing-sensitive improvements.
- Accelerator-aware optimization for 68020/030/040/060 Amigas.
- Profiling and performance qualification.
- Stable compatibility database and release qualification.

## Guiding principle

Do not emulate a 68k instruction when the host 68k can safely execute it directly. Emulate or translate the machine around the CPU only to the degree required by the workload, while always retaining a path toward stronger isolation and compatibility.
