Audit basis, 2026-10-14

Authoritative product: challanger2000/125A-MixEngine-Final. User confirmed Gumroad FULL V3.0.0 package 125A-MixEngine-V3-v3.0.0-Windows-x64-Gumroad-FINAL on v3.0.0-development; V1/V2 obsolete. Source HEAD: 23b4ddb669f02dbeb780a87a997522d98c7f426c. Audit branch: audit/v3.0.0-release-2026-10-14. No release tags/current-release manifest found. GitHub release API returned Forbidden, so build artifacts/logs not yet independently verified. Engineering Demo rollout table is stale per user.

Demo HEAD 541f427519e53174a0ea9f751ea2c591b342b4eb diverges at V2 commit 0055d0bbe05ef9502a71e1c81fca3b56ac6930c0. V3 metadata does not establish V3 implementation: it lacks V3 console/tape/tube/vinyl models and many tests. Inspect license resolver, timeline, integration and tests independently; do not wholesale merge. Mac branch has alpha narrowing/routing corrections and zoom commits; inspect for applicable fixes.

Potential issues requiring reproduction: cold wet state/shared dry-wet latency line on global bypass; no getTailSamples override; partial mutation on truncated state; setProcessing performs large state reset; unsmoothed output gain. Intentional V3 base coloration and programme-aware Level Match contradict stale docs; commit history records intentional decisions, so do not alter them as defects without product approval.
