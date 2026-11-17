E2SM-RC: slice PRB quota
========================

NORI implements one E2SM-RC control: the slice-level PRB quota of O-RAN.WG3.E2SM-RC
v03.00, section 7.6.3 (Control Style 2 "Radio Resource Allocation Control", Control
Action 6 "Slice-level PRB quota"), with the RAN parameters of section 8.4.3.6.

Control header
--------------

E2SM-RC Control Header **Format 1**:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Field
     - Value expected by NORI
   * - ``ric-Style-Type``
     - 2
   * - ``ric-ControlAction-ID``
     - 6
   * - ``ueID``
     - Any (decoded and logged; the quota applies to the whole cell)
   * - ``ric-ControlDecision``
     - Optional, logged

Requests with Action 6 and a style other than 2 are answered with
**RIC Control Failure**, cause ``action-not-supported``. In the specification, Action 6
of Style 1 is "DRB split ratio control", which NORI does not implement. Earlier NORI
versions accepted Style 1 with RAN parameter IDs 1 to 6; that format is no longer
accepted.

Control message
---------------

E2SM-RC Control Message **Format 1** with the following RAN parameter tree:

.. code-block:: text

   1  RRM Policy Ratio List                       LIST
      +- RRM Policy Ratio Group (list item)        STRUCTURE
         +- 3  RRM Policy                          STRUCTURE
         |     +- 5  RRM Policy Member List        LIST
         |           +- 6  RRM Policy Member       STRUCTURE (list item)
         |                 +- 7  PLMN Identity     ELEMENT, OCTET STRING (3), optional
         |                 +- 8  S-NSSAI           STRUCTURE
         |                       +- 9  SST         ELEMENT, OCTET STRING (1) or INTEGER
         |                       +- 10 SD          ELEMENT, OCTET STRING (3), optional
         +- 11 Min PRB Policy Ratio                ELEMENT, INTEGER (0..100)
         +- 12 Max PRB Policy Ratio                ELEMENT, INTEGER (0..100)
         +- 13 Dedicated PRB Policy Ratio          ELEMENT, INTEGER (0..100)

Decoding rules (``model/ric-control-message.cc``):

* each RRM Policy Ratio Group produces one quota per RRM Policy Member, and all
  members of the group share its three ratios;
* missing ratios take the defaults min = 0, max = 100, dedicated = 0;
* SST is read as INTEGER or as the first octet of an OCTET STRING; SD as INTEGER or a
  3-octet OCTET STRING (TS 38.473);
* a group without members is ignored. If no quota is extracted, NORI answers
  **RIC Control Failure**, cause ``control-message-invalid``;
* unknown parameter IDs inside a group are logged and skipped.

The quotas are applied by ``E2Interface::ApplySlicingControl()``, which converts them
to ``SlicePRBQuota`` and calls ``NrRLMacSchedulerOfdma::SetSlicingParameters()``. If
the gNB does not use ``NrRLMacSchedulerOfdma``, NORI answers **RIC Control Failure**,
cause ``control-failed-to-execute``. See :doc:`scheduler` for the meaning of the three
ratios.

Example
-------

Two slices, SST 1 with 80% dedicated and SST 2 with 20% dedicated:

.. code-block:: text

   ControlHeader-Format1: ric-Style-Type=2, ric-ControlAction-ID=6
   ControlMessage-Format1:
     1 RRM Policy Ratio List
       group 0: 3 { 5 [ 6 { 8 { 9 SST=0x01 } } ] }, 11 min=0, 12 max=100, 13 ded=80
       group 1: 3 { 5 [ 6 { 8 { 9 SST=0x02 } } ] }, 11 min=0, 12 max=100, 13 ded=20

NORI log (``NS_LOG=E2Interface=level_info``):

.. code-block:: text

   [E2SM-RC] Slice quota: SST=1 SD=0 min=0 max=100 dedicated=80
   [E2SM-RC] Slice quota: SST=2 SD=0 min=0 max=100 dedicated=20
   RAN Slicing control directive from RIC queued (2 slice quotas, 1 pending)
   Applying RAN Slicing control directive from RIC (requestorId=..., 2 slice quotas)
   Applied PRB quota: SST=1, dedicated=80%, min=0%, max=100%
   Applied PRB quota: SST=2, dedicated=20%, min=0%, max=100%
   [E2SM-RC] RIC Control Acknowledge sent: requestorId=... instanceId=... ranFunctionId=300

The ``min`` printed by "Applied PRB quota" is the value received. The scheduler
raises it to the dedicated ratio when it is lower (see :doc:`scheduler`).

Other request types
-------------------

``RicControlMessage`` also recognizes traffic steering and QoS requests from older
code paths. NORI only logs them; no action is taken.
