#pragma once

#include "ccc-data-models.h"
#include "nr-rl-mac-scheduler-ofdma.h"
#include "ns3/ptr.h"
#include "ns3/net-device.h"

namespace ns3 {

class CccIndicationMessage {
public:
    /**
* @brief Constructs the JSON payload of the CCC (CellDU) indication.

* @param netDev NetDevice of gNB.
* @param plmnId PLMN ID (string encoded by E2Term).
* @param scheduler Optional pointer to the RL scheduler. If provided,
* the current PRB quotas will be included in bwp.partitionList.
* @return JSON serialized as std::string.
     */
    static std::string BuildPayload(
        Ptr<NetDevice> netDev,
        const std::string& plmnId,
        Ptr<NrRLMacSchedulerOfdma> scheduler = nullptr);
};

} // namespace ns3