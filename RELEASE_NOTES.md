# NORI release notes

## NORI v3.00 (ns-3.48, nr v5.1)

Branch `fix/e2sm-rc-spec-slice-quota`, commits `bf3581a` to `edd98f6`, plus the
documentation.

### Availability

- Docker image `nori:dev` (Ubuntu 24.04, built by `docker compose build`; published as
  `ghcr.io/lasseufpa/nori:3.00-dev` with the release tag `nori-v3.00`). The previous
  environment is kept as `nori-nori-dev:ns3.42-nr78b7179e`. Results of the two versions are not
  directly comparable (nr v4.0 to v5.1 changed RLC, HARQ, TB size, MIMO SINR and the
  scheduler base classes).

### New features

- E2SM-RC slice PRB quota aligned with E2SM-RC v03.00 (Style 2, Action 6, RAN
  parameters 1 to 13), with RIC Control Acknowledge and Failure.
- RIC Control Acknowledge for E2SM-CCC controls.
- `StartStagger` attribute and `nori-handover-scenario`.
- `--trafficScale` option in `nori-testbed-scenario`.
- User manual (Sphinx), API reference (Doxygen) and the `docs` GitHub Actions
  workflow.
- `ci` GitHub Actions workflow (build of `libnori` and the examples, smoke test of the
  examples that run without a RIC) and `image` workflow (images `nori:ci` and
  `nori:dev` on GHCR).

### Bugs fixed

- KPM throughput and volumes were 0 when the traffic did not use LCID 4.
- `RRU.PrbUsedDl` was the same for all UEs and assumed a fixed 139-PRB band.
- The first KPM report, RC quotas and CCC subscriptions could be delayed (or never
  applied) when the simulation ran slower than real time.
- Data race between the SCTP thread and the simulation in the CCC control path.
- With 80% + 20% dedicated quotas, a slice with 0% received one RBG (rounding).
- The ns-3 process exited (`exit(1)` in e2sim) or was killed by SIGPIPE when the RIC
  reset or closed the E2 association, for example after the simulation was restarted.
  The gNB now reconnects (`E2TermHelper::ReconnectInterval`).
- The E2SM-CCC RAN function was announced in the E2 Setup with the E2SM-KPM OID
  (`1.3.6.1.4.1.53148.1.3.2.2`); it now uses `1.3.6.1.4.1.53148.1.6.2.4`.

### Known issues

- Only DL quotas are enforced.
- The PLMN identity is fixed (`268413`).
- `DRB.BufferSize.Qos` is always 0; several measurements announced in the RAN function
  definition are not reported (see the KPM chapter of the manual).
- The RNTI to SST map is global and keyed by RNTI, which is not unique across cells.
- ACKs of different xApps collide in the RIC unless each xApp routes 12041 by its own
  `ricInstanceID`.
