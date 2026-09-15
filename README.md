# Amtari

**Native and hybrid Atari compatibility environment for classic Amiga systems.**

Amtari explores how far Atari ST-family software can be run on Amiga hardware by exploiting the shared Motorola 68k architecture. Rather than treating every workload as conventional full-system emulation, Amtari is designed around progressively deeper compatibility modes: native execution where possible, hybrid trapping/virtualization where needed, and fuller hardware emulation for software that directly depends on Atari hardware.

## Goals

- Run Atari ST-family software on classic Amiga systems.
- Reuse the host 68k CPU directly where technically safe and useful.
- Translate or virtualize Atari OS and hardware interfaces instead of emulating the CPU unnecessarily.
- Keep the architecture modular so compatibility can grow from well-behaved GEM/TOS applications toward hardware-sensitive software.
- Support accelerated Amigas without tying the design to one CPU generation.

## Planned compatibility modes

1. **Native / API mode** — native 68k execution with GEMDOS/BIOS/XBIOS/AES/VDI compatibility services where practical.
2. **Hybrid mode** — native 68k execution combined with trapping and emulation of Atari-specific hardware behaviour.
3. **Full compatibility mode** — deeper machine emulation for software that cannot safely use the native or hybrid paths.

## Machine scope

Initial development targets the Atari ST family. The roadmap then expands through STE-class systems and later 68030-era machines. **Atari Falcon is an explicit long-term target**, including Falcon-specific video, audio and DSP subsystems once the common ST-family foundation is mature.

See [ROADMAP.md](ROADMAP.md) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Current status

M0 and M1 are qualified. M2 establishes a qualified minimal TOS/GEM application substrate: GEMDOS console/filesystem/process services, TOS PRG loading and relocation, a bounded 68000 execution core for the current compiler-generated corpus, process/heap isolation, BIOS console/drive services, and XBIOS Random/Gettime/Settime support.

CI exercises both the host regression suite and real cross-compiled m68k C programs. M2 does not require or redistribute proprietary Atari TOS ROM images; qualification uses synthetic and redistributable test inputs.

The next development phase is M3: VDI/AES abstractions, input translation, an AmigaOS-oriented display backend, and the first redistributable GEM application compatibility corpus.

## License

MIT — Copyright (c) Ploos AS.
