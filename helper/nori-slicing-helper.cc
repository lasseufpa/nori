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

// Static member definition
std::map<uint16_t, uint8_t> NoriSlicingHelper::m_rntiToSst;

uint8_t
NoriSlicingHelper::GetSstForRnti(uint16_t rnti)
{
    auto it = m_rntiToSst.find(rnti);
    if (it == m_rntiToSst.end())
    {
        return 0; // unknown
    }

    return it->second;
}

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

    // RNTI-to-slice mapping (per-slice lists, still used by the scheduler)
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

    // Update single-source-of-truth mapping RNTI -> SST
    RegisterSstMapping(sliceUeRntiMap);

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

void
NoriSlicingHelper::RegisterSstMapping(const std::vector<std::vector<uint32_t>>& sliceUeRntiMap)
{
    // Clear previous mapping before installing a new configuration
    m_rntiToSst.clear();

    for (size_t sliceIdx = 0; sliceIdx < sliceUeRntiMap.size(); ++sliceIdx)
    {
        // Semantic: slice index 0 -> SST=1, slice index 1 -> SST=2,
        // any other index -> SST=0 ("unknown" / not represented).
        uint8_t sst = 0;
        if (sliceIdx == 0)
        {
            sst = 1;
        }
        else if (sliceIdx == 1)
        {
            sst = 2;
        }
        else
        {
            NS_LOG_WARN("[NoriSlicingHelper] Slice index " << sliceIdx
                         << " has no SST mapping (only {1,2} supported); marking as unknown.");
        }

        for (uint32_t rnti32 : sliceUeRntiMap[sliceIdx])
        {
            uint16_t rnti = static_cast<uint16_t>(rnti32);
            if (sst == 0)
            {
                // Keep the RNTI unmapped (sst=0) to signal "unknown".
                continue;
            }

            m_rntiToSst[rnti] = sst;
            NS_LOG_INFO("[NoriSlicingHelper] Register SST=" << static_cast<uint32_t>(sst)
                        << " for RNTI=" << rnti);
        }
    }
}

} // namespace ns3
