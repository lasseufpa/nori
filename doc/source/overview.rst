Overview
========

Purpose
-------

NORI lets researchers test xApps against an ns-3 simulation instead of a radio
testbed. Each simulated gNB opens an SCTP association with the E2 Termination of a
Near-RT RIC and behaves as an E2 node:

* it answers the E2 Setup procedure and announces three RAN functions;
* it accepts RIC Subscriptions and sends periodic RIC Indications with E2SM-KPM
  measurements;
* it accepts RIC Control Requests (E2SM-RC and E2SM-CCC) that change the PRB quota
  of each network slice in the MAC scheduler, and answers with RIC Control
  Acknowledge or RIC Control Failure.

The xApps do not need to know that the E2 node is simulated. The same xApp can be
pointed to NORI and to a real gNB (for example OCUDU) by changing only the E2 node
it subscribes to.

Supported versions
------------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Component
     - Version
   * - ns-3
     - ns-3.48
   * - nr (5G-LENA)
     - v5.1
   * - E2AP
     - ASN.1 v03.01, compiled into the OSC ``e2sim`` library (commit ``da6f82f``)
   * - E2SM-KPM
     - ASN.1 v03.00 (RAN function ID 200)
   * - E2SM-RC
     - ASN.1 v03.01; Control Style 2, Action 6 as in E2SM-RC v03.00, 7.6.3 and
       8.4.3.6 (RAN function ID 300)
   * - E2SM-CCC
     - JSON encoding (``asn/E2SM-CCC-v07.00.json``), ``O-RRMPolicyRatio``
       (RAN function ID 4)
   * - Near-RT RIC
     - O-RAN SC Release I

The previous environment (ns-3.42 with nr commit ``78b7179e``) is kept as the Docker
image tag ``nori-nori-dev:ns3.42-nr78b7179e``. Results of the two versions are not
directly comparable: between nr v4.0 and v5.1 the RLC, HARQ, TB size, MIMO SINR and
scheduler models changed.

Module layout
-------------

.. code-block:: text

   nori/
     model/      E2 interface, E2 termination, service model codecs, slicing scheduler
     helper/     E2TermHelper, NoriSlicingHelper, indication message helpers
     examples/   simulation scenarios (see Examples)
     asn/        ASN.1 definitions compiled into the e2sim library image
     doc/        this manual and the Doxygen configuration
     Dockerfile, docker-compose.yml

Main classes
------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Class
     - Role
   * - ``E2TermHelper``
     - Installs NORI on a gNB: creates ``E2Termination`` and ``E2Interface``,
       registers the three RAN functions and connects the traces.
   * - ``E2Termination``
     - E2 agent of one gNB. Wraps the ``e2sim`` library, owns the SCTP
       association and runs its receive loop in a separate thread.
   * - ``E2Interface``
     - Logic of the E2 node: handles subscriptions and controls, builds the
       KPM indications and applies the slice quotas.
   * - ``NoriE2Report``
     - Collects per-UE MAC/PHY statistics (PDUs, MCS, SINR, RBs x symbols) from the
       ``RxPacketTraceUe`` trace.
   * - ``NrRLMacSchedulerOfdma``
     - OFDMA scheduler that splits the RBGs among slices with dedicated, minimum
       and maximum ratios.
   * - ``NoriSlicingHelper``
     - Maps UEs (RNTI) to slices (SST) and configures the scheduler after the
       random access procedure.
   * - ``KpmFunctionDescription``, ``RicControlFunctionDescription``,
       ``CccFunctionDescription``
     - RAN function definitions sent in the E2 Setup Request.
   * - ``KpmIndicationHeader``, ``KpmIndicationMessage``, ``RicControlMessage``,
       ``CccControlMessage``, ``CccIndicationMessage``
     - Encoders and decoders of the service model payloads.

Limitations
-----------

* Only downlink PRB quotas are enforced (``AssignDLRBG``). Uplink scheduling uses the
  round-robin of the base scheduler.
* Quotas apply to the primary bandwidth part of the gNB.
* The PLMN identity announced by ``E2TermHelper`` is fixed to ``268413``.
* ``DRB.BufferSize.Qos`` is reported as 0 at UE level (not implemented).
* ``DRB.UEThpDl`` divides the delivered volume by the whole reporting interval, not
  by the active transmission time of each burst (TS 28.552, 5.1.1.3.1). Bursty
  traffic is underestimated.
* With ``RealtimeSimulatorImpl`` the simulation can run slower than real time when
  the load is high. Controls are applied at the simulation time at which they
  arrive (see :doc:`architecture`), so the xApp period in wall-clock time maps to a
  shorter simulated period.
* ``nori-embb-urllc-scenario`` and ``nori-simple-rl-sched`` are not built
  (commented out in ``examples/CMakeLists.txt``).
