# NORI API and behavior changes

This file lists the changes that can break user scenarios or xApps, in the format of
the nr (5G-LENA) `CHANGES.md`. User-visible features and fixed bugs are in
`RELEASE_NOTES.md`.

## NORI v3.00 (ns-3.48, nr v5.1)

### New API

- `E2TermHelper` attribute `StartStagger` (Time, default 0): cell *N* starts its E2
  Setup at `(N - 1) x StartStagger`.
- `E2Interface` attribute `RcControlDrainPeriod` (Time, default 1 ms): period of the
  event that handles the subscriptions and controls queued by the SCTP thread.
- `E2Interface::StartSctpQueueDrain()`: starts that event. `E2TermHelper` calls it;
  scenarios that install the E2 objects by hand must schedule it from the simulation
  thread.
- `E2Interface::SendRcControlOutcome()`: RIC Control Acknowledge or Failure for E2SM-RC.
- `E2Interface::GetUePdcpTxBytes()`, `GetUePdcpDlRxBytes()`, `GetUePdcpUlRxBytes()`:
  cumulative PDCP counters of a UE summed over all its DRBs.
- `NoriE2Report::GetMacRbSymbolsUeSpecific()` and `GetLastResetTime()`.
- Example `nori-handover-scenario` (3 gNBs, X2 handover) and option `--trafficScale` of
  `nori-testbed-scenario`.
- Example `nori-print-attributes`: prints the attribute tables of the manual.
- `FunctionDescription::m_oid` and `E2SimMod::register_e2sm(id, definition, oid)`:
  service model OID of a RAN function, used in the E2 Setup Request.

### Changes to existing API

- ns-3.48 and nr v5.1 are required (previously ns-3.42 and nr `78b7179e`).
- `IndicationMessageHelper` derives from `SimpleRefCount` instead of `Object`; create
  it with `Create<>()`.
- Scenarios: nr v4.2+ QoS names (`NrQosFlow`, `NrQosRule`, `ActivateDedicatedQosFlow`),
  `RbOverhead` set through `NrHelper` before installing the gNB, `CsiFeedbackFlags`
  instead of `EnableMimoFeedback`.

### Changed behavior

- **E2SM-RC:** the slice PRB quota is Control Style 2, Action 6, with RAN parameter IDs
  1 to 13 of E2SM-RC v03.00, 8.4.3.6. Style 1 and the old IDs 1 to 6 are rejected with
  RIC Control Failure (`action-not-supported`). xApps written for the old format must
  be updated.
- **E2SM-RC:** every control is answered with RIC Control Acknowledge or Failure
  (previously no answer). xApps sharing the RIC must route message type 12041 by
  `ricInstanceID`.
- **E2SM-KPM:** `reportingPeriod` (Event Trigger Format 1) of the subscription sets the
  report period; `E2Periodicity` is only the fallback. `granulPeriod` is decoded from
  Action Definition Formats 1 to 5 and the Format 3 messages carry it.
- **E2SM-KPM:** `DRB.UEThpDl` is the PDCP volume received by the UE divided by the
  elapsed time (it was the volume offered to the gNB of LCID 4 only, often 0).
  PDCP volumes sum all DRBs.
- **E2SM-KPM:** `RRU.PrbUsedDl` is computed from the RBs x symbols of each TB and the
  BWP geometry (it assumed 139 PRBs and a 720 kHz RB).
- **Threading:** KPM and CCC subscriptions and RC and CCC controls are applied on the
  simulation thread at the current simulation time (previously scheduled from the
  SCTP thread with a wall-clock timestamp).
- **Scheduler:** `NrRLMacSchedulerOfdma::AssignDLRBG` works on RBG indices (nr v4.0+).
  Without slices it uses the round-robin allocation (previously it allocated nothing).
- **Cell IDs:** with several gNBs the IDs are 1, 2, 3, ... (they were 1, 3, 5, ...),
  which changes the E2 node names.
- **E2 Setup:** the `ranFunctionOID` of each RAN function item comes from its
  function description (`FunctionDescription::m_oid`) instead of a table indexed by
  RAN function ID. E2SM-CCC (ID 4) is announced with `1.3.6.1.4.1.53148.1.6.2.4`; it
  was announced with the E2SM-KPM OID. KPM and RC OIDs are unchanged.
