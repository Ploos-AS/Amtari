# Amtari Architecture

## Core idea

Amiga and Atari ST-family computers share the Motorola 68k instruction-set family but differ substantially in memory maps, operating-system interfaces and peripheral hardware. Amtari therefore separates **guest CPU execution** from **guest machine services**.

The project should not assume that all Atari code can be executed natively. Native execution is an optimization and compatibility technique used only when guest state, privilege behaviour, addressing and host CPU semantics make it safe. Other workloads use hybrid or fuller emulation paths.

## Layers

```text
Atari application / guest software
              |
      Guest execution manager
       /          |          \
 native 68k     hybrid      emulated
       \          |          /
          trap / event core
          /             \
 OS services             HW devices
 GEMDOS BIOS XBIOS       Shifter/MFP/ACIA/YM/WD1772/...
          \             /
             Amiga host
       AmigaOS / custom HW / RTG/AHI
```

## Execution manager

The execution manager owns guest CPU state and decides which execution backend is valid for a workload. Its interfaces must not bake in one specific Amiga CPU. 68000 through accelerated 68020/030/040/060 systems should be considered explicitly as the design matures.

## Trap core

The trap core provides a stable boundary for GEMDOS, BIOS and XBIOS calls. Later AES/VDI services can sit above this layer. Guest pointers must be validated against the guest address-space model before host-side access.

## Hardware layer

Atari devices are represented behind narrow interfaces. Early ST targets include Shifter, MFP 68901, ACIA/IKBD, YM2149 and WD1772-related storage behaviour. STE, TT and Falcon extensions build on these interfaces instead of creating unrelated emulator cores.

## Falcon

Falcon support is planned as a first-class later machine profile. Its 68030 code can benefit from the same shared-ISA strategy, while VIDEL, Falcon audio/DMA and especially the Motorola 56001 DSP require dedicated models/backends. The DSP must remain architecturally independent of the 68k execution backend.

## ROMs and test material

Amtari source code must not contain proprietary Atari TOS ROM images or other non-redistributable firmware/software. Runtime qualification should use user-supplied legal firmware where required and redistributable/open test software wherever possible.
