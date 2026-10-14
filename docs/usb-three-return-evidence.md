# Three synth USB returns: configuration evidence

Static REA follow-up, 12 October 2026. This investigation sends no hardware commands. The connected-device matrix is separate evidence maintained by the parent task.

## Device setting

The installer's `remotedevice.xml` defines `Global USB/Virus Control USB IO Configuration`, range 0–2, default 0, MIDI page `Bank F (73) Global`, controller index 9. Its value list is:

| Value | Shipped label |
|---|---|
| 0 | 2 Outs / No Input |
| 1 | 3 Outs / No Input |
| 2 | 3 Outs / 1 Input |

The XML message template at lines 8744 onward produces this candidate parameter change for the current broadcast device ID 16, global part 0:

```
F0 00 20 33 01 10 73 00 09 01 F7
```

Value 1 selects three synth returns without the plug-in audio-input path. Value 0 in the same message selects the default two-return configuration. This is a documented parameter encoding, not a firmware-update command. **The XML alone does not establish persistence, acknowledge behavior, or an exact readback request.**

The local Gearmulator TI DSP initialization separately contains `F0 00 20 33 01 10 73 40 09 02 F7` with the comment `USB Mode = 3 out 1 in` (`third_party/gearmulator/source/axel/virusLib/microcontroller.cpp:126`). That is an internal DSP-facing sequence with part 0x40; it corroborates the meaning of controller 9 but is not a reason to substitute 0x40 for the original host XML's global part 0 on physical hardware.

## Original Access behavior

Original common-engine target SHA-256: `52bd4a7dc5fe6956b69a3a7fd79e9371cd083042947d58287f10a66d9f6dc47d`.

- Parameter binding `0xc2632` puts this parameter at engine field `+0x136598`. Evidence `ev_40f79a80464eb2ff851c129f17abe56d220eac09f2c0b70f2964ffc66f7c0961`.
- Connect path `0xbd3d8` creates a per-device preference domain `de.access-music.virus_ti.control.<device identity>`, registers `IO Configuration` with default 0, and reads that preference into the parameter. It also substitutes that value in the initialization list. Prior retained decompilation: `analysis/hardware-routing-init-rea.c`, lines 570–652. This is a host preference, not a readback of the pre-existing device setting.
- Initialization `0xab494` sends the collected global initialization list. It chooses `Output Select Control2` when configuration is 0 and `Output Select Control3` when nonzero. Evidence `ev_963efc8cf85a1c9b86112b295a2cf41d97b305cb00b3957ce28b2208ef820f66`, same prior decompilation.
- Parameter-change handler `0xa7e7e` saves the selected value back into the per-device `IO Configuration` preference. Evidence `ev_b62d6f63a3f61d48e37d4ecbbb5e0e1e4320fc79ad0c9aa05b4cf66ac838ab74`.
- Cleanup `0xbf542` sends USB MIDI/panel/sound-dump cable defaults and `Global Master/Mode = 0`, waits 350 ms, then sends mode 0 again. It does not explicitly restore controller 9. Evidence `ev_4edf26b44d34d32b4b227b6334a0bcc0bbeb38c4769077a38e6f3312fccb6eeb`, `analysis/hardware-routing-init-rea.c:178`.

The original application thus treats this as its own remembered plug-in configuration. It does not provide evidence that controller 9 returns to zero when mode 0 is sent, or that all device globals are restored. That would require firmware tracing or controlled measurement; do not claim either from the cleanup routine.

## Driver calls are ownership, not another device command

The same parameter handler calls `vc_setIntParam(1, config >= 1)` and `vc_setIntParam(2, config == 2)`. These initially looked like a second USB configuration path, but their complete implementations show client ownership only:

| Original call | Library mapping | Kernel result |
|---|---|---|
| Int parameter 1 | `PGOSXDevice::setInputsExclusive`, streaming dispatch 0x1e | Stores/clears the current IOUserClient pointer at shared driver object `+0x387c`. |
| Int parameter 2 | `PGOSXDevice::setOutputsExclusive`, streaming dispatch 0x20 | Stores/clears the pointer at `+0x3884`. |

Library evidence: `vc_setIntParam` at `0x2b00`, `ev_81bab6deb209e55388c065584cd9692f1a1b2feddbe74047a0b4f912b6264f0b`; setters `0x11a40`/`0x11ae0`, `ev_de309b77587b916d76eab477649d94769919d6d7eca3c0b7f223c4765d95c052` and `ev_4585151428d37e5f915ace798afaa3b681d9f2a347d86666c412b5087005b920`. Library SHA-256 `a65cdd0d3419949d1c64ec51fe84b10261128fc6ffe0e859769549a23ec4411f`.

Kernel `PGDevice::dispatchFunction` at `0x13644`, cases 0x1e/0x20, evidence `ev_4aca6700d4e6f91d5283652f3fbab505a3c95740b1ca95a55bdc34b5213ea2a9`. Kernel SHA-256 `7308f591d634625acfaa0019f218af99e04cf936b4dfe18227a0bbf28a67e539`. These branches make no USB device request. They do not establish a further command needed by the reconstruction using Apple's audio driver.

## Minimal implementation boundary

The current reconstruction omits controller 9 entirely and always exposes three output buses. The measured symptom of two working synth pairs and input-like signal in the third is consistent with the original default value 0. It is a strong configuration hypothesis, not a readback of the current value.

A narrowly scoped implementation can encode controller 9 correctly, select 1 during the plug-in session, and select a **known captured or explicitly established previous value** on cleanup. Its MIDI allowlist should restrict this to page 0x73, global part 0, index 9, values 0–2. Failed connection paths must also restore any changed configuration. Neither default 0 nor audio silence alone establishes an exact previous value.

For this bench session, the implementation decision is to select 1 while connected and explicitly normalize to 0 on cleanup, matching the observed two-return baseline. Reconnecting the previous build and repeating the same route/audio probe can establish functional return to that baseline. This is normalization with a functional restoration check, not a byte-exact snapshot/restore of an unknown original global setting.

No exact readback of controller 9 was found in this bounded original-host investigation. The known global request `0x35` is not proven to return this internal parameter, and prior sessions did not return all globals. Until restoration is established experimentally or by MCU firmware analysis, an unconditional setting/reset should not be described as complete preservation of the device's original configuration.

New evidence bundles: `analysis/rea-usb-configuration-evidence.json`, `analysis/rea-usb-exclusive-evidence.json`, `analysis/rea-usb-exclusive-kernel-evidence.json`. Reusable common-engine snapshot: `analysis/VirusControl-usb-configuration.rea-snapshot.json`.
