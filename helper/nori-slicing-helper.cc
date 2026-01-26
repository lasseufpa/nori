/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "nori-slicing-helper.h"

#include "ns3/log.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"
#include "ns3/nr-ue-mac.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NoriSlicingHelper");

void
NoriSlicingHelper::ScheduleSliceMapping(Time when,
                                        bool enableRanSlicing,
                                        const std::vector<int>& uesPerSlice,
                                        NetDeviceContainer gNbDevs,
                                        NetDeviceContainer ueDevs)
{
    if (!(enableRanSlicing && !uesPerSlice.empty()))
    {
        NS_LOG_INFO("RAN slicing disabled or no slices configured (NoriSlicingHelper)");
        return;
    }

    NS_LOG_INFO("[NoriSlicingHelper] Slice configuration event scheduled for t="
                << when.GetSeconds() << "s");

    Simulator::Schedule(when,
                        &NoriSlicingHelper::ConfigureSliceMapping,
                        enableRanSlicing,
                        uesPerSlice,
                        gNbDevs,
                        ueDevs);
}

void
NoriSlicingHelper::ConfigureSliceMapping(bool enableRanSlicing,
                                         std::vector<int> uesPerSlice,
                                         NetDeviceContainer gNbDevs,
                                         NetDeviceContainer ueDevs)
{
    if (!(enableRanSlicing && !uesPerSlice.empty()))
    {
        NS_LOG_INFO("RAN slicing disabled or no slices configured (ConfigureSliceMapping)");
        return;
    }

    NS_LOG_INFO("[NoriSlicingHelper::ConfigureSliceMapping] Starting slice configuration in the "
                "RL scheduler...");

    // Discover the actual RNTI of each UE (BWP 0)
    std::vector<uint32_t> ueRntis(ueDevs.GetN(), 0);
    for (uint32_t i = 0; i < ueDevs.GetN(); ++i)
    {
        Ptr<NrUeNetDevice> ueNetDev = ueDevs.Get(i)->GetObject<NrUeNetDevice>();
        if (!ueNetDev)
        {
            NS_LOG_WARN("[NoriSlicingHelper] UE device at ueDevs[" << i
                        << "] is not a NrUeNetDevice");
            continue;
        }

        Ptr<NrUeMac> ueMac = ueNetDev->GetMac(0);
        if (!ueMac)
        {
            NS_LOG_WARN("[NoriSlicingHelper] NrUeMac is null for UE[" << i << "]");
            continue;
        }

        uint16_t rnti = ueMac->GetRnti();
        ueRntis[i] = rnti;
        NS_LOG_INFO("[NoriSlicingHelper] UE[" << i << "] has RNTI " << rnti);
    }

    // RNTI-to-slice mapping
    std::vector<std::vector<uint32_t>> sliceUeRntiMap(uesPerSlice.size());
    uint32_t currentUeIdx = 0;

    for (size_t sliceId = 0; sliceId < uesPerSlice.size(); ++sliceId)
    {
        int numUesInSlice = uesPerSlice[sliceId];
        for (int k = 0; k < numUesInSlice; ++k)
        {
            if (currentUeIdx < ueDevs.GetN())
            {
                uint32_t rnti = ueRntis[currentUeIdx];
                if (rnti == 0)
                {
                    NS_LOG_WARN("[NoriSlicingHelper] UE[" << currentUeIdx
                                << "] still has RNTI 0 when configuring slices");
                }
                sliceUeRntiMap[sliceId].push_back(rnti);
                NS_LOG_INFO("[NoriSlicingHelper] Slice " << sliceId << " -> UE index "
                            << currentUeIdx << " RNTI " << rnti);
                currentUeIdx++;
            }
        }
    }

    // Configure mapping in each gNB
    for (uint32_t gNbIdx = 0; gNbIdx < gNbDevs.GetN(); ++gNbIdx)
    {
        Ptr<NrGnbNetDevice> gnbNetDev = gNbDevs.Get(gNbIdx)->GetObject<NrGnbNetDevice>();
        if (!gnbNetDev)
        {
            continue;
        }

        Ptr<NrMacScheduler> scheduler = gnbNetDev->GetScheduler(0);
        Ptr<NrRLMacSchedulerOfdma> rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);

        if (rlScheduler)
        {
            rlScheduler->SetSliceUeMapping(uesPerSlice.size(), sliceUeRntiMap);
            NS_LOG_INFO("[NoriSlicingHelper] Slice mapping configured on gNB "
                        << gNbIdx);
        }
        else
        {
            NS_LOG_WARN("[NoriSlicingHelper] Scheduler of gNB "
                        << gNbIdx << " is not NrRLMacSchedulerOfdma");
        }
    }
}

} // namespace ns3
