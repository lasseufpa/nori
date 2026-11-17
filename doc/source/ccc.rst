E2SM-CCC: cell configuration and control
========================================

The E2SM-CCC RAN function (ID 4) carries JSON payloads instead of ASN.1. NORI uses it
for two things: periodic indications with the cell configuration and the current
slice quotas, and controls that change the quotas through ``O-RRMPolicyRatio``.

RAN function definition
-----------------------

Sent in the E2 Setup Request:

.. code-block:: json

   {
     "ranFunctionName": {
       "ranFunctionShortName": "ORAN-E2SM-CCC",
       "ranFunctionServiceModelOID": "1.3.6.1.4.1.53148.1.6.2.4",
       "ranFunctionDescription": "Cell Configuration and Control"
     },
     "supportedSlices": [ {"sst": 1, "sd": "000001"}, {"sst": 2, "sd": "000002"} ]
   }

Subscription and indications
----------------------------

A RIC Subscription to RAN function 4 is accepted like a KPM subscription (first
REPORT or INSERT action). The first indication is sent by the simulation thread and
then every ``E2Periodicity`` seconds (attribute of ``E2Interface``). The indication
header is the string ``CCC``; the message is a JSON ``CccCellDuIndicationMessage``:

.. code-block:: json

   {
     "cellLocalId": 1,
     "operationalState": "ENABLED",
     "cellState": "ACTIVE",
     "nrPci": 1,
     "plmnInfoList": [
       { "plmnId": "...", "snssaiList": [ {"sst": 1, "sd": "000001"},
                                          {"sst": 2, "sd": "000002"} ] }
     ],
     "bwpList": [
       { "bwpContext": "DL", "subCarrierSpacing": "30", "numberOfRBs": 273,
         "partitionList": [
           { "snssai": {"sst": 1, "sd": "000001"},
             "prbQuota": 80, "minPRBRatio": 80, "maxPRBRatio": 100 }
         ] }
     ]
   }

* ``snssaiList`` lists the SSTs configured with ``NoriSlicingHelper``; the SD is the
  SST written with 6 digits. Without slices it lists SST 1 and 2.
* ``partitionList`` comes from ``NrRLMacSchedulerOfdma::GetCurrentSliceQuotas()``:
  ``prbQuota`` is the dedicated ratio.
* One ``bwpList`` entry is produced per bandwidth part of the gNB.

Control
-------

The control header is a JSON object. When it has ``controlHeaderFormat.ricStyleType``,
the value must be 2; other values are logged and the control is discarded. The
message can use two formats.

O-RAN format (``O-RRMPolicyRatio``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Recognized by the key ``controlMessageFormat``:

.. code-block:: json

   {
     "controlMessageFormat": {
       "listOfCellsControlled": [ {
         "cellGlobalId": { "plmnIdentity": {"mcc": "268", "mnc": "341"},
                           "nRCellIdentity": "000000001" },
         "listOfConfigurationStructures": [ {
           "ranConfigurationStructureName": "O-RRMPolicyRatio",
           "newValuesOfAttributes": {
             "ranConfigurationStructure": {
               "resourceType": "PRB_DL",
               "rRMPolicyMemberList": [
                 { "plmnId": {"mcc": "268", "mnc": "341"},
                   "snssai": {"sst": 1, "sd": "ffffff"} } ],
               "rRMPolicyMinRatio": 10,
               "rRMPolicyMaxRatio": 60,
               "rRMPolicyDedicatedRatio": 60
             } } } ] } ]
     }
   }

* Each member of ``rRMPolicyMemberList`` becomes one ``SlicePRBQuota`` with the
  ratios of its structure. Members with SST 0 are skipped.
* ``cellGlobalId`` is decoded but not checked: the control applies to the gNB that
  received it.
* Missing ratios default to min = 0, max = 100, dedicated = 0.
* ``maxMimoLayers`` (NORI extension, default 0): when greater than 0, NORI sets the
  ``RankLimit`` attribute of the PMI search of every UE in the simulation to this
  value. ``nori-mimo-demo`` uses it.

Legacy format
^^^^^^^^^^^^^

Recognized by the key ``bwpList``: the same structure as the indication, where each
``partitionList`` entry sets ``prbQuota`` (dedicated), ``minPRBRatio`` and
``maxPRBRatio`` for one SST. Kept for older xApps.

Applying the control
--------------------

The decoded quotas are queued and applied on the simulation thread by
``E2Interface::ApplyCccSlicingControl()``, which calls
``NrRLMacSchedulerOfdma::SetSlicingParameters()``. NORI then sends a
**RIC Control Acknowledge** with the request ID and RAN function ID of the request.
A control that does not yield any quota is logged and not acknowledged.

Typical log (``NS_LOG=E2Interface=level_info|prefix_time:CccControlMessage=level_info|prefix_time``):

.. code-block:: text

   +8.833159878s CCC RIC Control received.
   +8.833164864s CCC RICcontrolHeader: ricStyleType=2
   +8.833172741s CCC Control (O-RRMPolicyRatio): SST=1 ded=60% min=10% max=60% res=PRB_DL maxMimoLayers=0
   +8.833172741s CCC Control (O-RRMPolicyRatio): SST=2 ded=20% min=10% max=20% res=PRB_DL maxMimoLayers=0
   +8.833172741s CCC Control (O-RRMPolicyRatio): SST=3 ded=20% min=10% max=20% res=PRB_DL maxMimoLayers=0
   +8.833182188s CCC Control queued (1 pending).
   +8.834000000s Applying CCC control from RIC (requestorId=1, 3 quotas)
   +8.834000000s RIC control Acknowledgement successfully sent to reqId=1
