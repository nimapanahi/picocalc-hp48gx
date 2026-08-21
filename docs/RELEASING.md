# Release process

This project follows Semantic Versioning. Prereleases use lowercase dotted
identifiers, for example `1.1.0-rc.1`; Git tags add the conventional `v`
prefix: `v1.1.0-rc.1`. Stable releases use tags such as `v1.0.0`.

1. Update `HP48GX_VERSION` and `HP48GX_VERSION_DISPLAY` in
   `src/platform/version.h`.
2. Move the pending changelog entries under a dated version heading.
3. Run `tests/run_host_tests.sh`.
4. Cross-build both the ROM-free template and a private local ROM-bearing test
   image. Confirm that locally inserting the ROM into the template produces a
   byte-for-byte identical UF2.
5. Confirm that Git contains no ROM, unpacked ROM, saved state, or
   ROM-containing UF2.
6. Commit, create an annotated tag, and push both commit and tag.
7. Create a GitHub release from the tag; mark it as a prerelease only for an
   `rc` tag. Attach the ROM-free template and SHA-256 checksums. Never attach
   an HP ROM or ROM-containing UF2 unless explicit redistribution permission
   has been independently verified.

The final candidate becomes `v1.0.0`; later compatible features increment the
minor version and fixes increment the patch version.
