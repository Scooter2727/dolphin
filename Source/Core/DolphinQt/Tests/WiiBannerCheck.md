# Channel intro compatibility checks

`wiitogether-banner-check` is excluded from normal builds and is not part of the
application package. With `ENABLE_TESTS=ON`, build that target and run its executable
beside Dolphin's deployed Qt libraries.

- No arguments: run synthetic archive, texture, layout, animation, rendering and
  invalid-input assertions. Fixtures contain no commercial game data.
- `--inspect <game-or-opening.bnr> [frame.png]`: load real local banner data,
  print a diagnostic summary, optionally render its frame at two seconds.
- `--preview [game-or-opening.bnr]`: open the production channel preview widget;
  without a path it uses the synthetic test banner. The two buttons close this
  utility instead of launching an emulator.

The reader supports U8/IMET/IMD5, LZ77 type 0x10, Wii TPL textures through
Dolphin's existing texture decoder, BRLYT pane hierarchies, BRLAN pane transforms,
visibility and pane opacity with Hermite/step keys, plus intro and loop sequences.
Windows audio uses bounded PCM16 WAV or BNS DSP ADPCM decoding with loop points.
Disc images use Dolphin's volume abstraction; WAD content zero is decrypted with
the ticket's title key. Extracted `.bnr` files can also be read directly.

This is an initial desktop renderer, not a complete emulation of NW4R/GX.
Multi-texture/indirect materials, material animation, vertex-color animation,
texture-coordinate animation, window panes, Wii bitmap fonts, and AIFF audio
are not reproduced faithfully yet. Unsupported input falls back to the game
cover while keeping Play Game and Back to Menu available. Some recognized but
partially supported effects show a compatibility note instead.

Initial real-image check: US Mario Kart Wii (RMCE01), 28 panes, 20 textures,
70-frame intro, 899-frame loop, stereo BNS audio at 32 kHz. This is a compatibility
check, not a claim of pixel-perfect output or broad game coverage.

Format references used for this independently implemented reader:

- https://wiibrew.org/wiki/Opening.bnr
- https://wiibrew.org/wiki/Wii_Animations
- https://github.com/wiidev/usbloadergx/tree/enhanced/source/banner
- https://github.com/vgmstream/vgmstream/blob/master/src/meta/bns.c
