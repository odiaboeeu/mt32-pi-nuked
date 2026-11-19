# mt32-pi-nuked

This is an independent community-maintained derivative of mt32-pi. It is not an official mt32-pi, Nuked-SC55, Nuked-MT32, Munt, or FluidSynth release.

The project is maintained as **mt32-pi-nuked** and extends [mt32-pi](https://github.com/dwhinham/mt32-pi) with bare-metal ports of [Nuked-SC55](https://github.com/nukeykt/Nuked-SC55) and [Nuked-MT32](https://github.com/nukeykt/Nuked-MT32), while preserving the original Munt and FluidSynth backends.

The current target is the **Raspberry Pi 4 in 64-bit mode**. The result is a single mt32-pi firmware that can switch between four synthesizer backends:

- Munt
- FluidSynth
- Nuked-SC55
- Nuked-MT32

This work is an integration and port of existing emulators. It is not a replacement for, or an official release of, the upstream projects.

## Important status notes

### Raspberry Pi 4 is the supported target

This development branch is intended for the Raspberry Pi 4. Nuked-SC55 is especially CPU-intensive because it emulates the original processors and PCM hardware at a low level.

A Raspberry Pi 3 was tested during development, but it does not provide enough processing performance to run Nuked-SC55 reliably, even after the optimizations attempted for this port. Munt, FluidSynth, and some less demanding configurations may still work on earlier boards, but the complete four-module firmware described here should be treated as a Raspberry Pi 4 build.

Raspberry Pi 5 support has not been validated by this project.

### Nuked-MT32 implementation status and native RV2 reverb

Nuked-MT32 remains a work in progress, but its upstream implementation now includes native emulation of the MT-32 RV2 reverb processor. This port uses the RV2 implementation developed by [nukeykt](https://github.com/nukeykt/Nuked-MT32), together with the corresponding external reverb ROM image.

The previous Munt-derived external reverb path is no longer used by the Nuked-MT32 backend. Munt remains available as an independent synthesizer backend in the same firmware.

Nuked-MT32 upstream primarily targets the second-generation MT-32, identified in this project as **MT-32 NEW**, using 2.xx Control ROMs. The NEW implementation is the current fidelity target for the low-level LA32, MCU, and RV2 emulation.

Support for the first-generation MT-32, identified as **MT-32 OLD**, using 1.xx Control ROMs, is an experimental extension maintained by this project. It includes adaptations for the older ROM banking, LA32 register addressing, and model-specific reverb ROM.

The OLD implementation is usable and has been tested on a Raspberry Pi 4, but it should not be expected to provide the same hardware fidelity as MT-32 NEW. Differences may remain in output level, PWM volume behavior, timing, and other model-specific hardware details. OLD support should be considered experimental unless and until the model is implemented or formally validated by the Nuked-MT32 upstream author, nukeykt.

PWM output scaling from the NEW implementation is currently bypassed for MT-32 OLD because applying the same scaling produced an incorrect lower output level in hardware testing. This is a model-specific compatibility measure pending further documentation and validation.

### Nuked-SC55 performance work

The Raspberry Pi 4 port uses optimization work derived from the [J.C. Moyer Nuked-SC55 fork](https://github.com/jcmoyer/Nuked-SC55). Without that optimization work, the SC-55 backend was not practical on the Raspberry Pi 4 in this bare-metal integration.

## Main changes in this fork

- Added a bare-metal Nuked-SC55 backend.
- Added SC-55 MkI and SC-55 MkII model selection.
- Added a dedicated producer path for Nuked-SC55 audio generation.
- Added a bare-metal Nuked-MT32 backend.
- Integrated the native Nuked-MT32 RV2 reverb implementation.
- Added automatic selection of model-specific OLD and NEW RV2 reverb ROMs.
- Added experimental support for first-generation MT-32 1.xx Control ROMs.
- Restored the upstream ring-modulation first-partial suppression required for correct timbre balance.
- Added native Nuked-MT32 audio rendering and optional resampling from 32 kHz to the configured system sample rate.
- Added MIDI short-message and SysEx handling for Nuked-MT32.
- Added Nuked-MT32 master-volume control through Roland SysEx.
- Added firmware LCD text, active-part indicators, and nine-part level meters for Nuked-MT32.
- Added standard and alternate MT-32 MIDI channel assignments.
- Added independent reversed-stereo configuration for Nuked-MT32.
- Added exact Nuked-MT32 Control ROM selection for versions 1.04, 1.05, 1.06, 1.07, 2.04, 2.06, and 2.07.
- Kept Munt, FluidSynth, Nuked-SC55, and Nuked-MT32 available in the same firmware.
- Disabled automatic creation of `sc55.log` on the SD card.
- Updated the development base to Circle Step 51, FluidSynth 2.6.1, and Munt 2.8.2.

Always check the recorded submodule commits in the branch being built, because dependency revisions may change as the development branch evolves.

## Required hardware

- Raspberry Pi 4
- A reliable power supply appropriate for the Raspberry Pi 4 and attached hardware
- FAT32-formatted SD card
- Supported audio output, preferably a compatible I2S DAC
- MIDI input through USB, GPIO MIDI, serial MIDI, or the MiSTer user-port solution supported by mt32-pi
- Optional supported LCD or OLED display

## Power warning for MiSTer user-port HAT users

A Raspberry Pi 4 generally requires more power than the Raspberry Pi 3-class boards commonly used with earlier mt32-pi installations.

If a HAT obtains power through the MiSTer user port, confirm that the HAT, cable, MiSTer power supply, and power path can safely provide the required current. An insufficient or unstable supply may cause undervoltage, throttling, audio interruptions, lockups, failed USB initialization, corrupted SD-card writes, or unexpected resets.

If stability problems occur:

1. Test the Raspberry Pi 4 with a known-good dedicated power supply.
2. Check the mt32-pi display or log output for undervoltage or CPU-throttling warnings.
3. Avoid assuming that a setup stable with a Raspberry Pi 3 will automatically be stable with a Raspberry Pi 4.
4. Verify the electrical design and documentation of the specific HAT before powering the Raspberry Pi 4 through the MiSTer user port.

## SD-card layout

A typical card layout is:

```text
/
|-- config.txt
|-- kernel8-rpi4.img
|-- mt32-pi.cfg
|-- firmware/
|-- overlays/
|-- roms/
|   |-- pcm_mt32.rom
|   |-- mt32_reverb_old.bin
|   |-- mt32_reverb_new.bin
|   |-- mt32_1_04_control.rom
|   |-- mt32_1_05_control.rom
|   |-- mt32_1_06_control.rom
|   |-- mt32_1_07_control.rom
|   |-- mt32_2_04_control.rom
|   |-- mt32_2_06_control.rom
|   |-- mt32_2_07_control.rom
|   |-- ctrl_cm32l_1_00.rom
|   |-- ctrl_cm32l_1_02.rom
|   |-- pcm_cm32l.rom
|   `-- sc55/
|       |-- mk1/
|       |   |-- rom1.bin
|       |   |-- rom2.bin
|       |   |-- waverom1.bin
|       |   |-- waverom2.bin
|       |   `-- waverom3.bin
|       `-- mk2/
|           |-- rom_sm.bin
|           |-- rom1.bin
|           |-- rom2.bin
|           |-- waverom1.bin
|           `-- waverom2.bin
`-- soundfonts/
    `-- your-soundfont.sf2
```

Only install ROM images legally obtained from hardware you own or from another source that you are legally permitted to use. ROM images are not included with this project.

## Nuked-SC55 ROM files

The integrated backend currently focuses on SC-55 MkI and SC-55 MkII operation.

### SC-55 MkII

Place the SC-55 MkII files in `roms/sc55/mk2/`.

```text
roms/sc55/mk2/
|-- rom_sm.bin
|-- rom1.bin
|-- rom2.bin
|-- waverom1.bin
`-- waverom2.bin
```

Expected file sizes:

```text
rom_sm.bin      4 KiB
rom1.bin       32 KiB
rom2.bin      512 KiB
waverom1.bin    2 MiB
waverom2.bin    1 MiB
```

Folder names and filenames must match exactly, including lowercase letters.

### SC-55 MkI

Place the SC-55 MkI files in `roms/sc55/mk1/`.

```text
roms/sc55/mk1/
|-- rom1.bin
|-- rom2.bin
|-- waverom1.bin
|-- waverom2.bin
`-- waverom3.bin
```

Expected file sizes:

```text
rom1.bin       32 KiB
rom2.bin      256 KiB
waverom1.bin    1 MiB
waverom2.bin    1 MiB
waverom3.bin    1 MiB
```

Do not place the MkI files directly in `roms/sc55/`.
The files must be stored in the `mk1` subdirectory.

The exact MkI firmware revision depends on the legally obtained ROM images supplied by the user.

## Nuked-MT32 ROM files

Nuked-MT32 uses one shared MT-32 PCM ROM, one selected Control ROM, and the RV2 reverb ROM corresponding to the selected MT-32 generation.

### Shared PCM ROM

```text
pcm_mt32.rom
```

Expected size:

```text
524288 bytes
```

### Supported Control ROM versions

```text
mt32_1_04_control.rom
mt32_1_05_control.rom
mt32_1_06_control.rom
mt32_1_07_control.rom
mt32_2_04_control.rom
mt32_2_06_control.rom
mt32_2_07_control.rom
```

Expected sizes:

```text
MT-32 1.xx Control ROM:  65536 bytes
MT-32 2.xx Control ROM: 131072 bytes
```

The scanner validates ROM identity from the recognized ROM content. The filenames above are the recommended convention for organization, but a correctly named file with invalid content will not be accepted as the requested version.

### Native RV2 reverb ROMs

The OLD and NEW MT-32 generations use different RV2 reverb ROM images. Both files should preferably be stored in the `roms` directory. For compatibility, the firmware also checks the SD-card root:

| MT-32 generation | Original ROM filename | Required filename |
| --- | --- | --- |
| MT-32 OLD, Control ROM 1.xx | `r15179857.ic13.bin` | `mt32_reverb_old.bin` |
| MT-32 NEW, Control ROM 2.xx | `r15179917.ic19.bin` | `mt32_reverb_new.bin` |

Expected size for each file:

```text
32768 bytes
```

Required SD-card paths:

```text
roms/mt32_reverb_old.bin
roms/mt32_reverb_new.bin
```

The firmware selects the reverb ROM automatically:

```text
MT-32 1.xx -> mt32_reverb_old.bin
MT-32 2.xx -> mt32_reverb_new.bin
```

The old generic filename `mt32_reverb.bin` is no longer used. Separate filenames prevent an OLD reverb ROM from being paired accidentally with a NEW Control ROM, or vice versa.

Original ROM filenames are provided only to help identify dumps obtained legally from hardware owned by the user. ROM images are not distributed with this project.

Munt may also use CM-32L, CM-64, or LAPC-I compatible ROM sets. This is one reason Munt remains available alongside Nuked-MT32.

## Configuration

Edit `mt32-pi.cfg` on the SD card.

### Munt

The existing `[mt32emu]` section is reserved for Munt:

```ini
[mt32emu]
gain = 1.0
reverb_gain = 1.0
resampler_quality = good
midi_channels = standard
rom_set = old
reversed_stereo = off
```

Common `rom_set` values for Munt are:

```text
old
new
cm32l
```

Munt and Nuked-MT32 use independent ROM selections. For example, Munt can use `cm32l` while Nuked-MT32 uses firmware `2.07`.

For Nuked-MT32, 2.xx firmware represents the primary NEW-model fidelity target. The 1.xx selections enable the experimental OLD-model implementation.

### Nuked-MT32

```ini
[nuked_mt32]
midi_channels = standard
rom_set = 2.07
reversed_stereo = off
```

Valid exact firmware selections are:

```text
1.04
1.05
1.06
1.07
2.04
2.06
2.07
```

MIDI channel modes:

```text
standard   Parts 1 through 8 use MIDI channels 2 through 9; Rhythm uses channel 10
alternate  Parts 1 through 8 use MIDI channels 1 through 8; Rhythm uses channel 10
```

Stereo modes:

```text
reversed_stereo = off  Keep the native MT-32 output orientation
reversed_stereo = on   Swap the final left and right output channels
```

Native RV2 reverb is part of the Nuked-MT32 low-level emulation and is not controlled by the previous external `reverb = on` or `reverb = off` option.

When selected, the display reports the actual recognized Control ROM, for example:

```text
Nuked-MT32 1.04
Nuked-MT32 2.07
```

### Nuked-SC55

```ini
[sc55]
model = mk2
debug = off
```

Supported model values in this integration:

```text
mk1
mk2
```

When `debug = on`, the display can show runtime counters used for diagnosing the audio producer and ring buffer:

```text
R  Ring-buffer occupancy
U  Underrun count
P  Samples produced per second
C  Samples consumed per second
```

The firmware no longer creates `sc55.log` automatically on the SD card.

### FluidSynth

FluidSynth remains configured through the normal `[fluidsynth]` section. At least one legally distributable or user-supplied SoundFont must be available in the `soundfonts` directory.

Example:

```ini
[fluidsynth]
soundfont = 0
polyphony = 200
gain = 0.2
reverb = on
chorus = on
```

### Large SoundFont warning

SoundFonts larger than approximately 60 MB may fail to load, substantially increase boot time, or cause the system to stop responding during startup.

The result can depend on the SoundFont, its internal structure, the available memory, and the specific Raspberry Pi 4 model. In some configurations, a large SoundFont may prevent the normal boot process from completing or may cause the Raspberry Pi 4 to freeze.

This limitation is still under investigation. If startup becomes unusually slow, FluidSynth fails to initialize, or the system freezes during boot, test again with a smaller SoundFont, preferably below 60 MB.

## Resampling and timing-sensitive backends

For the lowest latency and most predictable timing, avoiding resampling is recommended whenever the audio output device supports the native sample rate of the selected backend.

This recommendation is especially important for Nuked-SC55, which is more sensitive to timing and processing delays. Resampling adds processing cost and buffering and may increase latency or interfere with consistent real-time operation on heavily loaded Raspberry Pi 4 configurations.

When troubleshooting delayed MIDI response, timing instability, audio-buffer underruns, or playback slowdown, test first with resampling disabled.

## Selecting modules

The configured physical button can cycle through all available modules:

```text
Munt
FluidSynth
Nuked-SC55
Nuked-MT32
```

The modules can also be selected using mt32-pi custom SysEx messages.

```text
F0 7D 03 00 F7 = Switch to Munt
F0 7D 03 01 F7 = Switch to FluidSynth
F0 7D 03 02 F7 = Switch to Nuked-SC55
F0 7D 03 03 F7 = Switch to Nuked-MT32
```

Additional custom SysEx commands:

```text
F0 7D 01 00 F7 = Select the Munt MT-32 Old ROM set
F0 7D 01 01 F7 = Select the Munt MT-32 New ROM set
F0 7D 01 02 F7 = Select the Munt CM-32L ROM set

F0 7D 04 00 F7 = Disable reversed stereo
F0 7D 04 01 F7 = Enable reversed stereo

F0 7D 02 XX F7 = Select SoundFont index XX

F0 7D 00 F7 = Reboot mt32-pi
```

Exact Nuked-MT32 firmware versions are selected in `mt32-pi.cfg`, not with the Munt ROM-set SysEx command.

## Building for Raspberry Pi 4

Clone the repository and initialize all submodules:

```bash
git clone --recursive https://github.com/odiaboeeu/mt32-pi-nuked.git
cd mt32-pi-nuked
git submodule update --init --recursive
```

Build the 64-bit Raspberry Pi 4 image:

```bash
make clean
make BOARD=pi4-64
```

The resulting image is:

```text
kernel8-rpi4.img
```

Copy the Raspberry Pi 4 image to the root of the SD card as:

```text
kernel8-rpi4.img
```

The Raspberry Pi 4 section in `config.txt` must select it:

```ini
arm_64bit=1
armstub=armstub8-rpi4.bin
kernel=kernel8-rpi4.img
```

Build requirements and toolchain setup remain based on the original mt32-pi and Circle documentation.

## Dependency updates

This development branch includes integration work for newer project revisions than the original mt32-pi release line:

- Circle Step 51
- FluidSynth 2.6.1
- Munt 2.8.2

Circle provides the Raspberry Pi bare-metal runtime, drivers, multicore support, filesystems, USB, audio, and other platform services. FluidSynth provides SoundFont synthesis. Munt provides the established MT-32 family emulation backend. Nuked-MT32 now uses the native RV2 reverb implementation from its own upstream project.

Because these components are included as submodules or imported components, the exact commit recorded by the checked-out branch is authoritative for a reproducible build:

```bash
git submodule status
```

## Known limitations

- Raspberry Pi 4 is the supported and validated target for the complete integration.
- Raspberry Pi 3 does not have enough processing performance for reliable Nuked-SC55 operation in this port.
- Raspberry Pi 5 has not been validated by this project.
- Nuked-MT32 remains work in progress even though native RV2 reverb emulation is now integrated.
- MT-32 NEW with 2.xx Control ROMs is the current Nuked-MT32 fidelity target.
- MT-32 OLD with 1.xx Control ROMs is an experimental extension and should not be expected to match original OLD hardware with the same fidelity.
- PWM output scaling is bypassed for MT-32 OLD pending model-specific documentation and validation.
- SoundFonts larger than approximately 60 MB may load slowly, fail to initialize, or freeze some Raspberry Pi 4 configurations; this remains under investigation.
- Resampling may add latency and processing overhead, particularly with the timing-sensitive Nuked-SC55 backend.
- Nuked-SC55 and Nuked-MT32 require user-supplied ROMs that must be legally obtained.
- Low-level emulation is sensitive to CPU load, power quality, cooling, and audio-buffer timing.
- A MiSTer user-port HAT that was sufficient for a Raspberry Pi 3 may not provide a reliable power path for a Raspberry Pi 4.

## Troubleshooting

### Nuked-SC55 slows down or audio breaks up

- Use a Raspberry Pi 4.
- Test with a dedicated, known-good Raspberry Pi 4 power supply.
- Check for undervoltage and thermal-throttling warnings.
- Enable `[sc55] debug = on` and monitor `R`, `U`, `P`, and `C`.
- Confirm that the correct MkI or MkII ROM set is installed.

### Nuked-MT32 does not start

- Confirm that `pcm_mt32.rom` is present.
- Confirm that the exact Control ROM selected by `rom_set` is present.
- Confirm that `mt32_reverb_old.bin` is present when selecting a 1.xx Control ROM.
- Confirm that `mt32_reverb_new.bin` is present when selecting a 2.xx Control ROM.
- Verify that each reverb ROM is exactly 32768 bytes.
- Verify that the selected Control ROM value is one of the seven supported versions.
- Do not expect a missing Control ROM or reverb ROM to fall back silently to another model.

### The selected Nuked-MT32 version is incorrect

Check the configuration section name:

```ini
[nuked_mt32]
rom_set = 1.04
```

The `[mt32emu]` section controls Munt. The `[nuked_mt32]` section controls Nuked-MT32.

### `sc55.log` is not created

This is intentional. Automatic SC-55 log-file creation was disabled to avoid unnecessary persistent writes to the SD card. The visual debug counters remain available.

## Legal and ROM notice

This repository does not distribute Roland firmware, Control ROMs, PCM ROMs, wave ROMs, or other copyrighted firmware data. Users must provide legally obtained ROM images.

This project is intended to comply with the licenses of the projects it incorporates or derives from. Review the license files in this repository and in every submodule before redistributing source code, modified source code, binaries, firmware images, SD-card images, or hardware bundles.

At the time this README was prepared:

- mt32-pi is distributed under GPL-3.0.
- Nuked-SC55 is distributed under GPL-2.0-or-later.
- Nuked-MT32 is distributed under GPL-2.0.
- Munt uses LGPL-2.1 for the mt32emu library.
- Other dependencies retain their own licenses.

This README is informational and is not legal advice. The license texts included with the source code are authoritative.

## Project maintenance

mt32-pi-nuked is currently maintained by [odiaboeeu](https://github.com/odiaboeeu).

The contributors shown by GitHub include authors from the preserved mt32-pi commit history. Their presence in the contributors list does not imply current maintenance responsibility, project endorsement, or administrative access to this repository.

## Credits and acknowledgments

This project exists because of the work of many developers and contributors.

Special thanks to:

- [Dale Whinham](https://github.com/dwhinham), creator of mt32-pi, and everyone who contributed to the original project. mt32-pi provides the architecture, bare-metal integration, user interface, configuration system, MIDI support, display support, MiSTer integration, and the foundation on which this work is built.
- [nukeykt](https://github.com/nukeykt), creator of Nuked-SC55 and Nuked-MT32, for the extensive reverse-engineering work and for releasing both emulator codebases.
- [J.C. Moyer](https://github.com/jcmoyer), whose Nuked-SC55 fork and optimization work made the Raspberry Pi 4 SC-55 integration practical.
- [Keith Adler](https://github.com/keithadler), whose Nuked-MT32 development line contributed cross-platform, frontend, rendering, and testing work used during earlier stages of this port.
- The [Munt project](https://github.com/munt/munt) contributors for the established MT-32 family emulation backend retained by mt32-pi-nuked.
- The [FluidSynth project](https://github.com/FluidSynth/fluidsynth) contributors for the SoundFont synthesizer used by mt32-pi.
- The [Circle project](https://github.com/rsta2/circle) and circle-stdlib contributors for the Raspberry Pi bare-metal environment.
- Everyone who tested firmware images, reported regressions, documented hardware behavior, and compared emulation with original sound modules.

Please also consult the acknowledgment and license sections of every upstream repository. This fork does not claim ownership of the upstream emulator designs or reverse-engineering work.

## Upstream projects

- [Original mt32-pi project](https://github.com/dwhinham/mt32-pi)
- [Original Nuked-SC55 project](https://github.com/nukeykt/Nuked-SC55)
- [J.C. Moyer Nuked-SC55 fork](https://github.com/jcmoyer/Nuked-SC55)
- [Original Nuked-MT32 project](https://github.com/nukeykt/Nuked-MT32)
- [Keith Adler Nuked-MT32 fork](https://github.com/keithadler/Nuked-MT32)
- [Munt](https://github.com/munt/munt)
- [FluidSynth](https://github.com/FluidSynth/fluidsynth)
- [Circle](https://github.com/rsta2/circle)
