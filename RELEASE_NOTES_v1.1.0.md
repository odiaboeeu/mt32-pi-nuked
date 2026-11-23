# mt32-pi-nuked v1.1.0

This release integrates nukeykt's native Nuked-MT32 RV2 reverb and substantially reduces Nuked-SC55 audio buffering latency on Raspberry Pi 4.

## Highlights

### Native Nuked-MT32 RV2 reverb

- Native RV2 reverb replaces the previous external Munt-derived reverb path.
- Separate OLD and NEW reverb ROMs are selected automatically:
  - `r15179857.ic13.bin` -> `roms/mt32_reverb_old.bin`
  - `r15179917.ic19.bin` -> `roms/mt32_reverb_new.bin`
- MT-32 NEW with 2.xx Control ROMs remains the fidelity target.
- MT-32 OLD with 1.xx Control ROMs remains experimental and should not be expected to match original OLD hardware with the same fidelity.
- Restored ring-modulation first-partial suppression fixes the excessive OVER DRIVE guitar level heard in *Creatures In The Depth*.

### Lower Nuked-SC55 latency

The native producer queue was reduced from 32,768 to 512 frames. Start/restart and stop/rebuffer thresholds were reduced from 4,096/1,024 to 128/32 frames.

Hardware testing was performed on Raspberry Pi 4 using I2S at 48 kHz with `chunk_size=64`. Both SC-55 MkI and MkII remained stable, with no observed underruns and balanced production/consumption counters.

#### Buffering comparison

An I2S chunk of 64 frames at 48 kHz contributes approximately 1.33 ms. Combining one I2S chunk with the native SC-55 producer queue gives the following calculated buffering estimates:

| Model | v1.0.0 | v1.1.0 | Reduction |
| --- | ---: | ---: | ---: |
| SC-55 MkI | 513.33 ms | 9.33 ms | 504.00 ms (98.2%) |
| SC-55 MkII | 496.26 ms | 9.07 ms | 487.20 ms (98.2%) |

These are calculated buffering estimates, not measured end-to-end MIDI-to-audio latency. They exclude resampler, scheduling, driver, DAC, and MIDI transport latency.

## ROM requirements

Nuked-MT32 now requires the generation-specific RV2 ROM in addition to the selected Control ROM and shared PCM ROM:

```text
roms/mt32_reverb_old.bin
roms/mt32_reverb_new.bin
```

Each RV2 ROM must be exactly 32,768 bytes. ROM files are not distributed with this project.

## Additional notes

- SoundFonts larger than approximately 60 MB may delay boot, fail to initialize, or freeze some Raspberry Pi 4 configurations. Investigation is ongoing.
- Avoid additional resampling when the output device supports a backend's native rate. Nuked-SC55 is particularly sensitive to timing and buffering.
- Raspberry Pi 4 in 64-bit mode remains the supported target.
