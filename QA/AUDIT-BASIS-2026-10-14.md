Audit basis, 2026-10-14

Authoritative product: challanger2000/125A-MixEngine-Final. User confirmed Gumroad FULL V3.0.0 package 125A-MixEngine-V3-v3.0.0-Windows-x64-Gumroad-FINAL on v3.0.0-development; V1/V2 obsolete. Source HEAD: 23b4ddb669f02dbeb780a87a997522d98c7f426c. Audit branch: audit/v3.0.0-release-2026-10-14. No release tags/current-release manifest found. GitHub release API returned Forbidden, so build artifacts/logs not yet independently verified. Engineering Demo rollout table is stale per user.

Demo HEAD 541f427519e53174a0ea9f751ea2c591b342b4eb diverges at V2 commit 0055d0bbe05ef9502a71e1c81fca3b56ac6930c0. V3 metadata does not establish V3 implementation: it lacks V3 console/tape/tube/vinyl models and many tests. Inspect license resolver, timeline, integration and tests independently; do not wholesale merge. Mac branch has alpha narrowing/routing corrections and zoom commits; inspect for applicable fixes.

Potential issues requiring reproduction: cold wet state/shared dry-wet latency line on global bypass; no getTailSamples override; partial mutation on truncated state; setProcessing performs large state reset; unsmoothed output gain. Intentional V3 base coloration and programme-aware Level Match contradict stale docs; commit history records intentional decisions, so do not alter them as defects without product approval.

Follow-up 2026-10-15: origin/main advanced independently to d9fdf322397dfa3891cf55f546dd8c78488bd15b while this audit ran. New CURRENT.md (d752c44) and AGENTS.md (3272fbd) explicitly identify owner-approved Windows FULL V3.0.0, separate inventory from verified provenance, and protect the active audit. The later main change removes the obsolete Mac pipeline; none changes the verified V3 development-source baseline. These new navigation files were read; the released V3 source and active audit were not merged with the old main DSP.

| Merkmal | FULL V3.0.0 | Demo-Zweig |
|---|---|---|
| Branch | v3.0.0-development | Demo |
| Commit | 23b4ddb669f02dbeb780a87a997522d98c7f426c | 541f427519e53174a0ea9f751ea2c591b342b4eb |
| Version / Status | V3.0.0, vom Nutzer als letzter fertiger Gumroad-FULL bestätigt | V3-Beschriftung, tatsächlich ältere V2-DSP-Abstammung |
| Release-Evidenz | Windows FINAL-Pipeline 37497801176; Artefakt 11427969306 | Kein als V3-FULL zugeordnetes Demo-Artefakt unabhängig verifiziert |
| Reihenfolge | Verbindliche Nutzer-Freigabezuordnung; Datum allein unerheblich | Neuere Demo-Zeitstempel ersetzen FULL nicht |
| Binäridentität | Archiv-/VST3-Bytes nicht heruntergeladen, NOT VERIFIED | Gemeinsames Binärmodell dort angelegt; Gleichheit zum V3-FULL NOT VERIFIED |
| Lizenzunterschied | V3-Basis ohne spätere Demo-Lizenzintegration | LicenseStatus / DemoTimeline / Badge integriert; keine pauschale Übernahme |
| Gemeinsame Verbesserungen | Reparaturen unabhängig auf V3 nachgewiesen | Timeline isoliert geprüft; platform-/licensing-spezifische Änderungen nicht übertragen |

Latest Engineering origin/main 3c0d73f2e59c08163322839b9e7b49781b997f90 was read for newly added release navigation and Windows-only rules. Release register corroborates the exact existing V3 FULL run/artifact. The earlier generic alpha/routing review predates the new policy; no Mac-specific source or baseline was used. Windows remains the sole commercial target; Linux native builds serve independent audit hosting only. No Mac build, package or release is produced.

Final evidence follow-up (15.10.2026): original run37497801176/source23b4ddb artifacts were independently read in existing-byte-only Actions run38658672841. Inner Gumroad ZIP SHA-256 7dfb8e9c088fe999eda3592a8d149135afd0e6511277cdd99db1684987e95731; Mix FX and Channel are AMD64 DLLs, version3.0.0, and moduleinfo CIDs match the audited source. Full records: evidence/full-binary-provenance.json. This completes artifact/source provenance beyond the initial blocked metadata lookup, but not identity to the user's separately named local FINAL ZIP or original functional quality. The pre-edit basis decision above remains unchanged.
