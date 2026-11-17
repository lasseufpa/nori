Slicing scheduler
=================

``NrRLMacSchedulerOfdma`` derives from ``NrMacSchedulerOfdmaRR`` and overrides the DL
RBG assignment (``AssignDLRBG``). It splits the RBGs of each beam among network
slices according to three ratios per slice, as in the 3GPP RRM policy
(TS 28.541, ``RRMPolicyRatio``):

* **dedicated**: share reserved for the slice, even when it has no data;
* **min**: share the slice can take before other slices take their minimum;
* **max**: upper bound for the slice.

The ratios are percentages of the RBGs available in the beam (``GetDlBitmask()``).

Enabling it
-----------

.. code-block:: c++

   nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrRLMacSchedulerOfdma"));
   // ... install gNBs and UEs, attach UEs ...

   std::vector<int> uesPerSlice = {1, 1, 1};     // UEs of slice 0, 1, 2 (in device order)
   std::vector<uint8_t> sstPerSlice = {1, 2, 3}; // SST of each slice
   NoriSlicingHelper::ScheduleSliceMapping(Seconds(0.1), true,
                                           uesPerSlice, sstPerSlice, gNbDevs, ueDevs);

At the given time, ``ScheduleSliceMapping`` reads the RNTI of each UE from its MAC
(BWP 0). The time must be after the random access of all UEs (0.1 s in the examples);
a UE without RNTI at that moment is mapped with RNTI 0. Then it:

* assigns the first ``uesPerSlice[0]`` UEs of ``ueDevs`` to slice 0, the next ones to
  slice 1, and so on;
* calls ``NrRLMacSchedulerOfdma::SetSliceUeMapping()`` on each gNB;
* registers the RNTI -> SST map used by the scheduler, the KPM
  ``DRB.NetworkSlicing.SST`` measurement and the CCC indications.

The RNTI -> SST map is global (``NoriSlicingHelper`` static member) and keyed by RNTI
only. RNTIs are unique per cell, not across cells, and change after a handover;
scenarios with several gNBs or with handover should not rely on per-SST quotas.

Until a quota is received, each slice has dedicated = 0, min = 0 and max = 100.
Without ``ScheduleSliceMapping`` (no slices) the scheduler behaves as
``NrMacSchedulerOfdmaRR``.

Applying quotas
---------------

``SetSlicingParameters(const std::vector<SlicePRBQuota>&)`` receives one entry per
SST (from E2SM-RC, E2SM-CCC or the scenario itself):

* the SST is mapped to the internal slice index through the RNTI -> SST map; if the
  SST is not found, ``SST - 1`` is used when it is a valid index;
* quotas for unknown SSTs are ignored with a warning;
* ``min`` is raised to ``dedicated`` and ``max`` to ``min`` when they are lower.

The quotas stay in effect until the next call. ``GetCurrentSliceQuotas()`` returns
them.

Allocation per slot
-------------------

For each beam, ``AssignDLRBG`` runs three phases. With *N* RBGs available:

.. code-block:: text

   share(p) = floor(N x p / 100)

   Phase 1 - dedicated   for each slice s: reserve share(ded[s]) RBGs (lowest indices
                         first) and allocate them to the UEs of s; unused RBGs stay idle.
                         Then remove RBGs from the pool until
                         ceil(N x sum(ded) / 100) RBGs are reserved in total.
   Phase 2 - min         for each slice s: take up to share(min[s] - ded[s]) RBGs
                         from the common pool.
   Phase 3 - max         for each slice s: take up to share(max[s] - min[s]) RBGs
                         from what is left in the pool.

Inside a slice, ``AllocateSliceRbgs`` serves the UEs in the round-robin order of the
base scheduler:

* RBGs are taken in increasing index order (wideband allocation);
* a UE keeps receiving RBGs until its TB covers its buffer, or at least reaches the
  minimum DCI size of 10 bytes;
* a UE that cannot reach 10 bytes when the budget or the RBGs run out gives its RBGs
  back to the set, so its DCI is not dropped.

Effects to keep in mind
^^^^^^^^^^^^^^^^^^^^^^^

* **Dedicated RBGs are never shared.** With dedicated ratios summing to 100%, the
  common pool is empty; if a slice has no data, its share is lost. A policy with
  smaller dedicated ratios and max = 100 lets the other slices use what is idle.
* **max = dedicated caps the slice.** This is what the E2SM-CCC test mode of the
  ``ccc-python`` xApp sends (60/20/20 with max equal to dedicated).
* **Rounding.** ``floor`` per slice plus the ``ceil`` of the total reserve keeps the
  dedicated total equal to the previous NORI versions. Without it, 80% + 20%
  dedicated left one RBG free, and a slice with 0% got it.
* **Quotas only limit when the cell is loaded.** If the offered traffic of a slice
  fits in its share, the quota has no visible effect on its throughput.

Uplink
------

Only the DL assignment is sliced. The UL uses the round-robin assignment of
``NrMacSchedulerOfdmaRR``.
