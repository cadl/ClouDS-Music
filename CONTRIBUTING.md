# Contributing to ClouDS Music

Thanks for helping improve ClouDS Music. The application targets both Old and
New Nintendo 3DS hardware, so changes must be designed for the smaller memory
and CPU budget of Old 3DS/2DS systems.

## Before opening a change

1. Search existing issues and open a focused issue for changes that affect UX,
   storage compatibility, network behavior, or hardware requirements.
2. Clone with submodules:

   ```sh
   git clone --recursive https://github.com/cadl/ClouDS-Music.git
   cd ClouDS-Music
   ```

3. Do not include Nintendo firmware, account credentials, media caches, crash
   dumps, or diagnostic logs. In particular, never commit or attach
   `auth.bin`, `MUSIC_U`, `dspfirm.cdc`, complete media URLs, or another user's
   account data.

## Development and tests

Host-side tests do not require devkitPro:

```sh
make host-test
```

With devkitARM configured, build with `make -j2`. Otherwise use the pinned
devkitPro container:

```sh
make emulator-build
```

Before submitting application code, run both commands. Changes involving
networking, audio/NDSP, SD-card I/O, input, memory pressure, or sleep/resume
also require the relevant checks in
[`docs/HARDWARE_TEST.md`](docs/HARDWARE_TEST.md). State clearly which tests ran
on Azahar and which ran on real hardware; emulator launch alone does not prove
audio output or Old 3DS compatibility.

## Engineering constraints

- Keep network, download, image-processing, and MP3 indexing work off the main
  render/input thread.
- Preserve cancellation and `.part` cleanup semantics.
- Do not disable TLS certificate or hostname verification.
- Do not bypass VIP, region, purchase, or takedown restrictions.
- Avoid large stack objects and whole-file media buffers.
- Keep the existing two-screen focus and standard `A/B/L/R/START` behavior.
- Do not mechanically reformat `external/` or `third_party/`.
- Update `THIRD_PARTY_NOTICES.md` and license files when adding code, assets, or
  build dependencies from another project.

## Pull requests

Keep each pull request focused and include:

- the user-visible behavior and motivation;
- regression and Old 3DS memory considerations;
- tests actually run and their results;
- screenshots for UI changes at native 400×240 or 320×240 resolution; and
- third-party source, version, checksum, and license changes when applicable.

By contributing, you agree that your contribution may be distributed under
the repository's MIT License. Third-party material remains under its original
license and must be identified explicitly.
