# Decisions log

| Date | Decision | Who | Notes |
|---|---|---|---|
| 2026-10-29 | The project targets the **Atari/Atarisoft 1983 Apple II Pac-Man** as identified in docs/RELEASE_IDENTITY.md; no revision number is claimed. | Claude (recorded; evidence in the image), user's earlier statement of the 1983 US release | The brief's "1982" is an open discrepancy, not a target. |
| 2026-10-29 | **TASK-023 (Taxman source) is logged in the project.** The user gave permission to *log* it. This is **not** permission to download, clone or read files from any host; any download needs a separate explicit approval (rule 2) after the host and license are known. | User | Browsing public web pages remains allowed. |
| 2026-10-29 | Tracking of reads before the program starts is discarded in the coverage tool; earlier "read as data" numbers were corrected (docs/DATA_REGIONS.md). | Claude | Found while working on TASK-008; disclosed in the ledger and the report. |
| 2026-10-29 | Work proceeds one ledger task per "go" even when the user lists several (e.g. "4, then 3"); each stops at its checkpoint (rule 11). | User's standing rule | |
| 2026-10-29 | **Provisional**: the verifier accepts (a) the exact supplied image (SHA-256 F9E19375...) or (b) any DOS-order 140K image whose `PAC-MAN` file body has SHA-256 EFF5B263.... No public hash corroborates these. **Confirmed by the user on 2026-10-29** (the rule, not a public hash). | Claude (recommendation), user (confirmation) | docs/VERIFIER_SPEC.md |
| 2026-10-29 | **PENDING user decision D1** (data source: local data pack built from the verified disk, re-created assets, or build-time embedding) and **D2** (code strategy: oracle-driven reconstruction or mechanical translation). Recommendations: D1-A, D2-A. D3-D5 default (Direct3D 11; XAudio2; cycle clock; CMake + build.bat) unless objected. | Claude (recommendation) | docs/PORT_PLAN.md |
