Architecture
============

Components
----------

.. code-block:: text

   +------------------------------- ns-3 process --------------------------------+
   |                                                                              |
   |  NrGnbNetDevice (5G-LENA)                                                    |
   |    | RRC (UE map, DRBs)  MAC scheduler  PHY traces   PDCP/RLC traces         |
   |    v                         ^              |              |                 |
   |  E2Interface  <--------------+--------------+--------------+                 |
   |    |  builds KPM indications, applies slice quotas                          |
   |    v                                                                         |
   |  E2Termination  ---- e2sim library ---- SCTP thread                          |
   +--------------------------------------------|---------------------------------+
                                                | SCTP (E2AP over port 36422/32222)
                                                v
   +--------------------------- Near-RT RIC (OSC Release I) ----------------------+
   |  E2Term -> E2Mgr / SubMgr / RtMgr -> RMR -> xApps (KPM, RC, CCC)             |
   +------------------------------------------------------------------------------+

``E2TermHelper::InstallE2Term()`` builds this chain for each gNB:

#. creates an ``E2Interface`` bound to the gNB ``NetDevice``;
#. enables the E2 PDCP and RLC statistics calculators (``NrBearerStatsCalculator``)
   and passes them to the ``E2Interface``;
#. creates an ``E2Termination`` with the RIC address, a local SCTP port
   (``E2LocalPort`` + cell ID), the cell ID as gNB ID and the encoded PLMN;
#. connects ``RxPacketTraceUe`` (PHY) to ``NoriE2Report``, ``DlDataSinr`` to the SINR
   map, and, 0.2 s later, the RLC ``TxPDU`` trace of each DRB;
#. registers the RAN functions: KPM (ID 200), RC (ID 300) and CCC (ID 4), each with
   a callback into the ``E2Interface``;
#. schedules ``E2Termination::Start`` at ``(cellId - 1) x StartStagger`` and
   ``E2Interface::StartSctpQueueDrain`` at time zero.

Both objects are aggregated to the gNB ``NetDevice`` and can be retrieved with
``GetObject<E2Interface>()`` and ``GetObject<E2Termination>()``.

E2 node identity
----------------

The RIC identifies an E2 node by PLMN and gNB ID. NORI uses:

* PLMN string ``268413``, fixed in ``E2TermHelper::InstallE2Term()`` and packed in 3
  octets (two digits per octet). The OSC RIC decodes it as MCC 268 / MNC 341, so the
  E2 node names start with ``gnb_268_341_``;
* gNB ID = cell ID of the gNB (``NrGnbNetDevice::GetCellId()``).

With nr v5.1 the cell IDs of consecutive gNBs are 1, 2, 3, ...; the OSC RIC names
the nodes ``gnb_268_341_00000001``, ``gnb_268_341_00000002``, and so on. With the
previous nr version the IDs were 1, 3, 5. xApps that select E2 nodes by name must use
the new names.

Threads and time
----------------

The ``e2sim`` library receives E2AP messages in its own thread (the *SCTP thread*).
The simulation runs in the main thread. When a subscription or a control arrives, the
callback runs on the SCTP thread.

Scheduling a simulation event from another thread has a side effect with
``RealtimeSimulatorImpl``: the event gets a wall-clock timestamp. If the simulation is
slower than real time, that timestamp is in the simulated future, and the first KPM
report or the quota is applied late, or never within the run.

NORI avoids this with a producer-consumer queue:

.. code-block:: text

   SCTP thread                              simulation thread
   -----------                              -----------------
   KPM/CCC subscription  --+
   RC control (decoded)  --+--> mutex + deque --> DrainSctpQueues() every
   CCC control (decoded) --+                      RcControlDrainPeriod (1 ms)
                                                    - start KPM reports
                                                    - start CCC indications
                                                    - apply quotas, send ACK/Failure

* The SCTP thread decodes the message and only pushes it to a queue
  (``m_pendingKpmSubscriptions``, ``m_pendingCccSubscriptions``,
  ``m_pendingRcControls``, ``m_pendingCccControls``), protected by one mutex.
* ``DrainSctpQueues()`` runs on the simulation thread every
  ``RcControlDrainPeriod`` (attribute of ``E2Interface``, default 1 ms of simulated
  time). It swaps the queues and acts at the current simulation time.
* ``StartSctpQueueDrain()`` must be scheduled from the simulation thread.
  ``E2TermHelper`` does this. If a scenario installs the E2 objects by hand and
  forgets it, NORI logs ``... queued but the drain event is not started``.

Consequences:

* a control takes at most one drain period (simulated time) to be applied after it
  is decoded;
* when the simulation runs at a fraction of real time, a fixed xApp period in
  wall-clock time corresponds to a shorter simulated period. In the testbed scenario
  with 580 Mb/s offered, a 10 s xApp period corresponds to about 0.5 s of simulated
  time.

Real-time operation
-------------------

Scenarios that talk to a RIC set ``SimulatorImplementationType`` to
``ns3::RealtimeSimulatorImpl`` (``--enableRealtime=true``; the testbed scenario also
does it when ``--enableE2=true``). The simulator then waits when it is ahead of the
wall clock and runs as fast as it can when it is behind. It never drops events.

The speed of a run depends on the offered traffic, the number of PRBs and the MIMO
configuration. See :doc:`user-guide` for the optimized build.
