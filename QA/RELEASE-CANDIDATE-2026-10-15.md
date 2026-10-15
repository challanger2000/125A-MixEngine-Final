# MixEngine V3 Windows candidate — persistent record (2026-10-15)

This file is an INTERNAL release-evidence record, not part of the customer ZIP.

- Product: 125A MixEngine V3, Windows x64 VST3, Mix FX and Channel.
- Current previously owner-approved FULL: v3.0.0 (`AGENTS.md` and `CURRENT.md` on `main`); remains unchanged.
- Last functionally audited source: `cee8e9123d31e265ee826cd3d7e81c13e8810170` on `audit/v3.0.0-release-2026-10-14`.
- Audit handoff: `d5fc02597951a3b290bb675c7464f0844584a687`, documentation-only after functional source.
- Candidate source/build branch: `release/v3.0.1-windows-candidate` (branch name is NOT version certification: VST3 internal version is still 3.0.0).
- Candidate ZIP naming: `125A-MixEngine-V3-v3.0.0-AUDIT-CANDIDATE-Windows-x64.zip`.
- Internal QA: `QA/AUDIT-HANDOFF-2026-10-15.md` and reports. Windows Channel validator passed; previous dense automation @192 kHz/64 samples showed missed deadlines (last audit run `38656678023`).
- Approval: NOT FINAL; customer release not authorized by QA PASS. No claim of passing unresolved gate.
- Required later: successful candidate build/run and artifact SHA-256, actual ZIP SHA-256, confirmed Studio One/owner acceptance and appropriately recorded risk decision, immutable archive/tag/release, `main` AGENTS/CURRENT sync and Engineering RELEASE-REGISTER/RELEASE-STATUS sync.
- GitHub Actions artifacts expire: do not rely on their URL alone as the permanent customer-archive location.

## Verified Windows candidate package build

- Workflow run: https://github.com/challanger2000/125A-MixEngine-Final/actions/runs/38660722997 — **SUCCESS** (build, existing release gates, Channel Validator, DE/EN manual generation, package extraction check).
- Exact workflow/source checkout: `6162cafd1c188f896a9a3cc04b3519bf63d816a0`.
- Actions artifact ID: `11939175957`.
- Actions artifact name: `125A-MixEngine-V3-v3.0.0-AUDIT-CANDIDATE-Windows-x64`.
- Actions artifact archive SHA-256: `5d0bc4d4123871edf3cefffd96c1c9201f3cfac00e96b0fb6b35e659fa2f5171` (the GitHub Actions outer archive, **not** the checksum of the ZIP contained inside).
- Actions artifact retention expiry: `2027-01-13T00:33:38Z`; a permanent release attachment is still necessary.
- Customer-facing files: Mix FX VST3, Channel VST3, corrected German and English PDF manuals, README, release notes, license, install guide, per-file SHA-256 manifest. Internal audit-status file is excluded.
- The existing build workflow did not run the outstanding dense-automation realtime gates again; its **SUCCESS does not supersede** the separately documented audit failure.
- Final customer package ZIP SHA-256, human DAW signoff and actual publication remain **NOT VERIFIED**. No FINAL/Gumroad-live claim from this candidate artifact.

## Independently inspected package bytes

- Inner customer-package ZIP: `125A-MixEngine-V3-v3.0.0-AUDIT-CANDIDATE-Windows-x64.zip`, **26,163,711 bytes**.
- Actual inner ZIP SHA-256: **`0e8a8357489c4dfe0c809128bc86f42a694d61c4798cb39f808ecdd935fdbfbb`**.
- Verified 44/44 inner file SHA-256 manifest entries against the ZIP contents; zero mismatch.
- Both .vst3 bundles and German/English PDF manuals included. Both PDFs have 4 pages and contain corrected Level Match guidance/90% zoom; neither contains the internal NOT FINAL audit status.
- Audit status belongs only to QA records and artifact naming. Package customer-facing README/notes/manuals exclude internal QA status.

## Windows V3.0.1 candidate — corrected embedded versions (2026-10-15)

- Successful Actions run: https://github.com/challanger2000/125A-MixEngine-Final/actions/runs/38663301857
- Exact Windows build source commit: `09bd8ad3c44c00122e6ab83a4e3ca5ec81687078`.
- Artifact ID: `11939932788`; Actions archive SHA-256: `ccd11f98f5430179cacfe4ab42714298d275d4157960e0301861380b5c3950ab`.
- Inner ZIP filename: `125A-MixEngine-V3-v3.0.1-AUDIT-CANDIDATE-Windows-x64.zip`.
- Independently inspected inner ZIP SHA-256: **`53dcb18b2bd6826a966119c2e0faeee7e20a1e314491a9584b0b70d43b40b460`** (26,163,863 bytes).
- Both `VST3/125A-MixEngine-V3.vst3` and `VST3/125A-MixEngine-V3-Channel.vst3` embed `moduleinfo.json` version `3.0.1` for plugin and controller classes. CMake version `3.0.1` drives the compiled factory version.
- 44/44 packaged SHA-256 manifest entries match ZIP contents; README, release notes and install text identify v3.0.1; both DE/EN manuals are present.
- Earlier v3.0.0 candidate entries in this document are **historical**, not current. Latest approved commercial FULL stays v3.0.0 pending owner final acceptance. Open realtime gate from prior audit remains documented; package/build SUCCESS does not erase it.
