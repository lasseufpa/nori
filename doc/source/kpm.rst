E2SM-KPM measurements
=====================

RAN function 200 (``ORAN-E2SM-KPM``, OID ``1.3.6.1.4.1.53148.1.3.2.2``) implements
E2SM-KPM v03.00 periodic reports.

Announced styles
----------------

.. list-table::
   :header-rows: 1
   :widths: 18 32 15 15 20

   * - Report style
     - Name
     - Action format
     - Indication format
     - Level
   * - 1
     - E2 Node Measurement
     - 1
     - 1
     - Cell
   * - 4
     - Common Condition-based, UE-level E2 Node Measurement
     - 4
     - 3
     - UE

Event trigger style 1 (periodic report, Event Trigger Format 1) is the only trigger.
As described in :doc:`e2ap`, every subscription receives one Format 1 and one
Format 3 indication per reporting period, with the measurements below.

Reporting window
----------------

All measurements cover the interval since the previous report of the same
subscription (one record per indication, ``granulPeriod`` = reporting period):

* MAC/PHY counters are accumulated by ``NoriE2Report`` from the
  ``RxPacketTraceUe`` trace (one entry per DL TB received by a UE) and reset after
  the UE-level message is built;
* PDCP volumes are differences of the cumulative counters of the E2 PDCP
  ``NrBearerStatsCalculator``, summed over all data radio bearers of the UE.

The node-level message is built before the UE-level message, so both read the same
window.

Cell-level measurements (Format 1)
----------------------------------

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Measurement
     - Unit
     - Computation
   * - ``DRB.PdcpSduVolumeDL``
     - kbit
     - PDCP DL volume transmitted by the gNB in the window, summed over UEs and DRBs.
   * - ``RRU.PrbUsedDl``
     - PRB
     - Sum over UEs of ``RRU.PrbUsedDl`` (UE level), rounded up.
   * - ``RRU.PrbAvailDl``, ``RRU.PrbAvailUl``
     - PRB
     - Number of RBs of the primary BWP (``NrGnbPhy::GetRbNum()``), for example
       273 at 100 MHz with numerology 1.
   * - ``TB.TotNbrDlInitial``
     - TBs
     - DL TBs with redundancy version 0 (first transmissions).
   * - ``TB.TotNbrDlInitial.Qpsk`` / ``.16Qam`` / ``.64Qam``
     - TBs
     - First transmissions with MCS 0-9 / 10-16 / 17-28.
   * - ``TB.ErrTotalNbrDl.1``
     - TBs
     - DL TBs with redundancy version > 0 (HARQ retransmissions).
   * - ``QosFlow.PdcpPduVolumeDL_Filter``
     - bytes
     - Sum of the DL TB sizes received by the UEs (MAC volume). Note that the
       UE-level measurement with the same name uses the PDCP volume in kbit.
   * - ``DRB.MeanActiveUeDl``, ``RRC.ConnMean``
     - UEs
     - Number of UEs in the gNB RRC (``UeMap``) at report time.
   * - ``CARR.PDSCHMCSDist.Bin1`` ... ``Bin6``
     - TBs
     - DL TBs with MCS 0-4, 5-9, 10-14, 15-19, 20-24, 25-29.
   * - ``L1M.RS-SINR.Bin34`` ... ``Bin127``
     - TBs
     - DL TBs with SINR <= -6, <= 0, <= 6, <= 12, <= 18, <= 24 and > 24 dB
       (bins named ``Bin34``, ``Bin46``, ``Bin58``, ``Bin70``, ``Bin82``,
       ``Bin94``, ``Bin127``).
   * - ``DRB.BufferSize.Qos``
     - bytes
     - Always 0 (not implemented).

UE-level measurements (Format 3)
--------------------------------

Each UE is identified by its IMSI, carried as AMF-UE-NGAP-ID of a gNB UE ID.

.. list-table::
   :header-rows: 1
   :widths: 30 12 58

   * - Measurement
     - Unit
     - Computation
   * - ``DRB.UEThpDl``
     - kbit/s
     - PDCP DL volume **received by the UE** since the previous UE-level report,
       summed over its DRBs, divided by the elapsed simulation time. On the first
       report the reference is the simulation start.
   * - ``QosFlow.PdcpPduVolumeDL_Filter``
     - kbit
     - PDCP DL volume **transmitted** by the gNB to the UE in the window, all DRBs.
   * - ``RRU.PrbUsedDl``
     - PRB
     - Average PRBs used by the UE in the window, rounded up (see below).
   * - ``TB.TotNbrDl.1``
     - TBs
     - All DL TBs of the UE (first transmissions and retransmissions).
   * - ``TB.TotNbrDlInitial`` (+ ``.Qpsk``, ``.16Qam``, ``.64Qam``)
     - TBs
     - As in the cell-level table, for one UE.
   * - ``TB.ErrTotalNbrDl.1``
     - TBs
     - HARQ retransmissions of the UE.
   * - ``CARR.PDSCHMCSDist.Bin1`` ... ``Bin6``
     - TBs
     - MCS distribution of the UE.
   * - ``L1M.RS-SINR.Bin34`` ... ``Bin127``
     - TBs
     - SINR distribution of the UE.
   * - ``DRB.BufferSize.Qos``
     - bytes
     - Always 0 (not implemented).
   * - ``DRB.NetworkSlicing.SST``
     - --
     - SST of the UE (``NoriSlicingHelper::GetSstForRnti()``), 0 if not mapped.
       NORI-specific measurement.
   * - ``HO.SrcCellQual.RS-SINR``
     - 3GPP index
     - Latest L3 SINR of the serving cell (``DlDataSinr`` trace), mapped with
       ``L3RrcMeasurements::ThreeGppMapSinr()``.
   * - ``DRB.EstabSucc.5QI``
     - DRBs
     - Number of data radio bearers of the UE (``DataRadioBearerMap``).

PRBs used by a UE
^^^^^^^^^^^^^^^^^

``RRU.PrbUsedDl`` follows TS 28.552 (mean PRB usage over the measurement period).
For a UE with RNTI *r*:

.. code-block:: text

   PrbUsedDl(r) = sum over DL TBs of r of (assigned RBs x OFDM symbols)
                  ----------------------------------------------------
                  slots in the window x symbols per slot

The RB and symbol counts come from ``RxPacketTraceUe``; the slot period and the symbols
per slot are read from the PHY of the primary BWP (``PrbGeometry``). Earlier versions
assumed a fixed 139-PRB band and TDMA allocation.

UE throughput
^^^^^^^^^^^^^

TS 28.552 (5.1.1.3.1) defines ``DRB.UEThpDl`` from the volume acknowledged by the UE,
divided by the active transmission time of each burst. NORI uses the PDCP volume
received by the UE and divides it by the whole interval. For constant-rate traffic the
two agree; for bursty traffic NORI underestimates the throughput.

Announced versus reported measurements
--------------------------------------

The RAN function definition lists the measurements per style, but the report content
is fixed by the code above. Differences in this version:

* announced but not reported: ``DRB.PdcpSduVolumeUL``, ``RRU.PrbUsedUl``,
  ``RRU.PrbTotDl``, ``RRU.PrbTotUl``, ``DRB.MeanActiveUeUl``, ``DRB.UEThpUl``,
  ``QosFlow.PdcpPduVolumeUL_Filter``, ``DRB.PdcpPduNbrDl.Qos``,
  ``DRB.PdcpPduNbrUl.Qos``, ``L1M.RS-SINR``;
* reported but not announced: the ``CARR.PDSCHMCSDist`` and ``L1M.RS-SINR`` bins,
  ``TB.ErrTotalNbrDl.1``, ``TB.TotNbrDl.1`` and ``DRB.EstabSucc.5QI``;
* reported in a style that does not announce them: ``QosFlow.PdcpPduVolumeDL_Filter``
  and ``DRB.BufferSize.Qos`` at cell level (announced only in style 4);
  ``RRU.PrbUsedDl`` and ``TB.TotNbrDlInitial`` (with modulation variants) at UE level
  (announced only in style 1).

Values sent as ``integer`` or ``real`` follow the type used in the code
(``AddNodeMeasurementInteger``/``Real``, ``AddUeMeasurementInteger``/``Real``):
``DRB.UEThpDl`` and ``HO.SrcCellQual.RS-SINR`` are real; the others are integer.
