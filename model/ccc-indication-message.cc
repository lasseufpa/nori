#include "ccc-indication-message.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nori-slicing-helper.h"
#include "ns3/log.h"

#include <iomanip>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("CccIndicationMessage");

std::string CccIndicationMessage::BuildPayload(
    Ptr<NetDevice> netDev,
    const std::string& plmnId,
    Ptr<NrRLMacSchedulerOfdma> scheduler)
{
    auto gnb = DynamicCast<NrGnbNetDevice>(netDev);
    if (!gnb) return "{}";

    CccCellDuIndicationMessage msg;
    msg.cellLocalId = gnb->GetCellId();
    msg.operationalState = "ENABLED";
    msg.cellState = "ACTIVE";
    msg.nrPci = gnb->GetCellId();

    CccPlmnInfo plmn;
    plmn.plmnId = plmnId;

    auto ssts = NoriSlicingHelper::GetConfiguredSsts();
    if (!ssts.empty()) {
        for (uint8_t sst : ssts) {
            std::ostringstream sd;
            sd << std::setw(6) << std::setfill('0') << static_cast<uint32_t>(sst);
            plmn.snssaiList.push_back({sst, sd.str()});
        }
    } else {
        plmn.snssaiList.push_back({1, "000001"});
        plmn.snssaiList.push_back({2, "000002"});
    }
    msg.plmnInfoList.push_back(plmn);

    std::vector<SlicePRBQuota> currentQuotas;
    if (scheduler) {
        currentQuotas = scheduler->GetCurrentSliceQuotas();
    }


    for (uint8_t bwpIdx = 0; bwpIdx < gnb->GetCcMapSize(); ++bwpIdx) {
        auto phy = gnb->GetPhy(bwpIdx);
        CccBwp bwp;
        bwp.bwpContext = "DL";
        bwp.subCarrierSpacing = std::to_string(15 * (1 << phy->GetNumerology()));
        bwp.numberOfRBs = phy->GetRbNum();

        for (const auto& q : currentQuotas) {
            CccPartition part;
            part.snssai.sst = q.sliceId;
            std::ostringstream sd;
            sd << std::setw(6) << std::setfill('0') << static_cast<uint32_t>(q.sliceId);
            part.snssai.sd = sd.str();
            part.prbQuota = static_cast<uint32_t>(q.dedicatePRBRatio);
            part.minPRBRatio = static_cast<uint32_t>(q.minPRBRatio);
            part.maxPRBRatio = static_cast<uint32_t>(q.maxPRBRatio);
            bwp.partitionList.push_back(part);
        }

        msg.bwpList.push_back(bwp);
    }

    nlohmann::json j = msg;
    return j.dump();
}

} // namespace ns3