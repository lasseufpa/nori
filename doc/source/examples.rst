Examples
========

The examples in ``examples/`` are built with ``--enable-examples``. Options are listed
with ``--PrintHelp``. All examples that talk to a RIC need ``--ipE2TermRic`` and
``--ipE2TermRicPort``; their defaults are addresses of old test clusters.

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Example
     - Purpose
   * - ``nori-testbed-scenario``
     - Equivalent of the LASSE OCUDU testbed: 1 gNB, 3 UEs, 3 slices, KPM + RC + CCC.
   * - ``nori-rc-slicing-demo``
     - 2 slices under saturation, PRB quotas through E2SM-RC.
   * - ``nori-ccc-slicing-demo``
     - Same topology, PRB quotas through E2SM-CCC.
   * - ``nori-handover-scenario``
     - 3 gNBs, 2 moving UEs, X2 handover seen by a KPM xApp.
   * - ``nori-benchmark-scenario``
     - Scalable topology for execution time measurements.
   * - ``nori-sample``
     - Minimal KPM example (1 gNB, 1 UE).
   * - ``nori-mimo-demo``
     - MIMO configuration; ``maxMimoLayers`` of E2SM-CCC changes the rank limit.

``nori-testbed-scenario``
-------------------------

Reproduces the radio configuration of the OCUDU testbed:

* 1 gNB, band n78 at 3.35001 GHz, 100 MHz, numerology 1 (273 PRBs), TDD pattern
  ``DL|DL|DL|DL|DL|DL|DL|S|UL|UL``, 20 dBm, 4T4R (gNB 2x2, UE 2x2), 64-QAM MCS table;
* 3 UEs at about 25 m, one per slice (SST 1, 2, 3), each with a dedicated DL bearer;
* ``NrRLMacSchedulerOfdma`` with ``NoriSlicingHelper`` mapping at 0.1 s;
* E2 periodicity 1 s.

Traffic (UDP OnOff, downlink):

.. list-table::
   :header-rows: 1

   * - UE
     - Slice
     - Rate
     - Packet
     - On / Off
   * - 0
     - SST 1, video streaming
     - 30 Mb/s
     - 1400 B
     - 1 s / 0 s
   * - 1
     - SST 2, social network
     - 10 Mb/s
     - 1200 B
     - 0.5 s / 0.5 s
   * - 2
     - SST 3, cloud gaming
     - 18 Mb/s
     - 1100 B
     - 1 s / 0.01 s

Options: ``--simTime`` (0 = until Ctrl+C, default 10), ``--enableE2``,
``--enableRealtime`` (forced on with E2), ``--ipE2TermRic``, ``--ipE2TermRicPort``,
``--statsInterval`` (period of the per-slice throughput printout, default 1 s) and
``--trafficScale`` (multiplies the three rates, default 1).

With the default traffic the cell is not loaded and PRB quotas have no visible effect.
``--trafficScale=5`` (150/50/90 Mb/s) saturates the cell.

``nori-rc-slicing-demo``
------------------------

1 gNB and 2 UEs (SST 1 and 2), each UE with saturating UDP traffic
(``--trafficRate``, default 100 Mb/s). An RC xApp sends
slice PRB quotas; the scheduler log shows the RBG split per slot. Real time is on by
default.

``nori-ccc-slicing-demo``
-------------------------

Same as ``nori-rc-slicing-demo`` with ``--trafficRate`` default 250 Mb/s, controlled by
an E2SM-CCC xApp (``O-RRMPolicyRatio``).

``nori-handover-scenario``
--------------------------

Three gNBs on a line (``--gnbDistance``, default 200 m) and two UEs moving back and
forth in opposite directions at ``--ueSpeed`` (default 20 m/s), each with
``--trafficRate`` DL traffic (default 5 Mb/s). Handover uses the A3 event on RSRP
(``--hysteresis`` 3 dB, ``--timeToTrigger`` 256 ms) and X2.

Every gNB has its own E2 node (``gnb_268_341_00000001`` to ``..._00000003``).
``--e2StartStagger`` (default 1 s) delays the E2 Setup of consecutive gNBs. Each
completed handover is printed as ``[HO] t=<s> IMSI=<imsi> -> cell <cellId>``, which is
the ground truth to compare with the E2 node in which the KPM xApp sees each IMSI.

``nori-benchmark-scenario``
---------------------------

UMi channel (TR 38.901) with presets: 1 = 2 gNBs / 10 UEs, 2 = 50 gNBs / 1 UE,
3 = 10 gNBs / 1 UE, 4 = 5 gNBs / 1 UE, 5 = 50 gNBs / 10 UEs (``--preset``;
``--gNbNum`` and ``--ueNum`` override). E2 and real time are off by default, so the
example measures the raw simulation speed.

``nori-sample`` and ``nori-mimo-demo``
--------------------------------------

Adapted from the CTTC examples ``cttc-3gpp-channel-simple-ran`` and
``cttc-nr-mimo-demo`` with an E2 termination on the gNB.

Not built
---------

``nori-embb-urllc-scenario``, ``nori-simple-rl-sched`` and ``oran-interface-example``
are commented out in ``examples/CMakeLists.txt``.
