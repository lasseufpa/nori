Validation
==========

Results obtained in November 2026 with the OSC RIC (Release I) and the xApps of the
LASSE OpenRAN project: KPM (``hw-python``), RC (``rc-python``) and CCC
(``ccc-python``). They check that NORI delivers what the xApps expect and that the
controls change the simulation.

KPM against FlowMonitor
-----------------------

``nori-testbed-scenario`` (default traffic), ``DRB.UEThpDl`` averaged over the run
against the FlowMonitor throughput of each UE:

.. list-table::
   :header-rows: 1

   * - Slice
     - ns-3.42 (KPM x FlowMonitor)
     - ns-3.48 + nr v5.1 (KPM x FlowMonitor)
   * - SST 1
     - 30.65 x 30.60 Mb/s
     - 30.64 x 30.59 Mb/s
   * - SST 2
     - 5.13 x 5.12 Mb/s
     - 5.13 x 5.12 Mb/s
   * - SST 3
     - 0 x 0 Mb/s
     - 0 x 0 Mb/s

The KPM reports arrived every 1000 ms (19 intervals of 1000 ms in 20 s). The KPM xApp
decoded all indications (``decode_errors=0``).

E2SM-RC quotas
--------------

``nori-rc-slicing-demo`` with saturating traffic and quotas 80/20 (dedicated) from the
RC xApp: the RBGs assigned to the two UEs were split 80.0/20.0 in every period, in
both directions of the quota, with ns-3.42 and with ns-3.48. Each RIC Control Request
was answered with a RIC Control Acknowledge; with the route ``12041|65535`` registered
by the RC xApp, all acknowledges reached it.

E2SM-CCC quotas
---------------

``nori-ccc-slicing-demo``: 15 of 15 controls applied and acknowledged by NORI.

``nori-testbed-scenario --trafficScale=10`` with the CCC xApp in test mode (60/20/20
with max equal to dedicated, rotating among SST 1, 2 and 3 every 10 s of wall-clock
time):

* before the first control (t = 2 to 8 s, no quotas) the cell delivered
  125 / 42 / 120 Mb/s (SST 1/2/3), about 288 Mb/s in total, with losses of 17 to 59%;
* after the controls (one every ~0.5 s of simulated time), the throughput of each
  slice followed the fraction of the 1 s interval in which it held 60%. Example: SST 3
  delivered 96.6 Mb/s in an interval where it held 60% for 0.48 s, and 66.8 Mb/s when
  it held 60% for 0.14 s;
* the total dropped to 180-220 Mb/s because dedicated RBGs are reserved even when
  unused and max = dedicated (see :doc:`scheduler`).

Handover
--------

``nori-handover-scenario`` (80 s, 3 gNBs, 2 UEs): 16 handovers. The sequence of E2
nodes in which the KPM xApp saw each IMSI (stored in InfluxDB) matched the
``HandoverEndOk`` trace. The cell IDs of the three gNBs were 1, 2, 3 (1, 3, 5 with
ns-3.42).

Execution time
--------------

Wall-clock time of ``nori-testbed-scenario --simTime=3`` without RIC and without real
time, same random seed, on the ``nori-dev-v51`` container (16 cores, one used):

.. list-table::
   :header-rows: 1

   * - ``--trafficScale``
     - default profile
     - optimized profile
     - speed-up
   * - 1
     - 14.9 s
     - 6.3 s
     - 2.4x
   * - 5
     - 26.6 s
     - 11.6 s
     - 2.3x

Both profiles produced the same throughput values. Upgrading from ns-3.42 to ns-3.48
made ``nori-rc-slicing-demo`` at 1 Gb/s about 8% faster (6.08 s against 5.64 s of
simulated time in 150 s of wall-clock time).

Not validated
-------------

* ``nori-sample``, ``nori-mimo-demo`` and ``nori-benchmark-scenario`` with E2 were only
  built, not run against the RIC after the upgrade to ns-3.48.
* Uplink measurements.
