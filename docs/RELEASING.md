# Release process

This project follows Semantic Versioning. Prereleases use lowercase dotted
identifiers, for example `1.1.0-rc.1`; Git tags add the conventional `v`
prefix: `v1.1.0-rc.1`. Alpha development builds use identifiers such as
`1.1.0-alpha.dev.3` and are normally distributed for device testing without a
stable release tag. Stable releases use tags such as `v1.0.0`.

1. Update `HP48GX_VERSION` and `HP48GX_VERSION_DISPLAY` in
   `src/platform/version.h`.
2. Move the pending changelog entries under a dated version heading.
3. Run `tests/run_host_tests.sh`.
   Ensure all Git submodules are initialized first.
4. Cross-build both the ROM-free template and a private local ROM-bearing test
   image. Confirm that locally inserting the ROM into the template produces a
   byte-for-byte identical UF2.
5. Run the exact-ROM host graphics/audio profiles, `--merge1-profile`, and
   `--calculator-profile`
   with `HP48_CAPTURE_DIR` set to separate empty capture directories. Build the
   labeled MP4s with `tools/make_validation_video.py` and
   `tools/make_calculator_validation_video.py`, inspect both complete videos,
   and publish them beside the private test UF2 and checksum. The first video
   must show the current graphics/control regression and a sound-producing
   graphics demo. The calculator video must show Port 1 memory before/after
   `MERGE1`, CAS, matrix, multi-function 2D plotting, and 3D parametric
   plotting. A private hardware handoff is not ready without both. These
   pre-flash checks can reject a broken build, but
   they do not replace a final PicoCalc panel, keyboard, and speaker check.
6. Confirm that Git contains no ROM, unpacked ROM, saved state, or
   ROM-containing UF2.
7. Commit, create an annotated tag, and push both commit and tag.
8. Create a GitHub release from the tag; mark it as a prerelease only for an
   `rc` tag. Attach the ROM-free template and SHA-256 checksums. Never attach
   an HP ROM or ROM-containing UF2 unless explicit redistribution permission
   has been independently verified.

`v2.0.0` marks the feature-complete main emulator feature set. `v2.1.0` adds
the supported virtual Port 1 user-memory expansion. Later compatible features
increment the minor version and fixes increment the patch version.

`HP48_CAPTURE_DIR` enables optional changed-frame PGM capture plus
`frames.csv`, `audio-edges.csv`, and profile-specific `markers.csv` metadata in
`tests/host_stubs.c`. Create the directory before running `rom_boot_smoke`;
normal host tests do not write captures when the variable is unset. Both video
builders intentionally label their footage as a host capture and reconstruct
captured calculator-level square waves at the firmware's 60% volume setting.
