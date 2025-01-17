// Copyright (c) 2025 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-rl-mac-scheduler-ofdma.h"

#include <ns3/log.h>
#include <ns3/nr-fh-control.h>

#include <algorithm>
#include <random>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("NrRLMacSchedulerOfdma");
NS_OBJECT_ENSURE_REGISTERED(NrRLMacSchedulerOfdma);

TypeId
NrRLMacSchedulerOfdma::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrRLMacSchedulerOfdma")
                            .SetParent<NrMacSchedulerOfdmaRR>()
                            .AddConstructor<NrRLMacSchedulerOfdma>();
    return tid;
}

NrRLMacSchedulerOfdma::NrRLMacSchedulerOfdma()
    : NrMacSchedulerOfdmaRR()
{
    NS_LOG_FUNCTION(this);
    numberSlices = 2;
    minRbPercSlices = {70, 30};
    dedicatedRbPercSlices = {30, 30};
    maxRbPercSlices = {100, 100};
    sliceUeRnti = {{1, 2}, {3, 4}};
}

NrMacSchedulerNs3::BeamSymbolMap
NrRLMacSchedulerOfdma::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    // RAN slicing addition
    std::vector<uint32_t> minRbPerSlicesOnly(minRbPercSlices.size());
    std::vector<uint32_t> maxRbPerSlicesOnly(maxRbPercSlices.size());
    std::vector<std::vector<UePtrAndBufferReq>> ranSliceUeVector(numberSlices);

    // Iterate through the different beams
    for (const auto& el : activeDl)
    {
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        uint32_t rbgAssignable = 1 * beamSym;
        FTResources assigned(0, 0);
        const std::vector<bool> dlNotchedRBGsMask = GetDlNotchedRbgMask();
        uint32_t resources = !dlNotchedRBGsMask.empty()
                                 ? std::count(dlNotchedRBGsMask.begin(), dlNotchedRBGsMask.end(), 1)
                                 : GetBandwidthInRbg();
        uint32_t total_resources = resources;
        NS_ASSERT(resources > 0);

        // RAN slicing addition
        uint32_t numberSlices = 2;

        for (uint16_t sliceIdx = 0; sliceIdx < numberSlices; sliceIdx++)
        {
            NS_ASSERT(dedicatedRbPercSlices[sliceIdx] <= minRbPercSlices[sliceIdx]);
            NS_ASSERT(minRbPercSlices[sliceIdx] <= maxRbPercSlices[sliceIdx]);
            minRbPerSlicesOnly[sliceIdx] =
                minRbPercSlices[sliceIdx] - dedicatedRbPercSlices[sliceIdx];
            maxRbPerSlicesOnly[sliceIdx] = maxRbPercSlices[sliceIdx] - minRbPercSlices[sliceIdx];

            // Creating UE vector for each slice
            for (const auto& ue : GetUeVector(el))
            {
                for (uint16_t rnti : sliceUeRnti[sliceIdx])
                {
                    if (ue.first->m_rnti == rnti)
                    {
                        ranSliceUeVector[sliceIdx].emplace_back(ue);
                        BeforeDlSched(
                            ue,
                            FTResources(rbgAssignable,
                                        beamSym)); // update UE metrics // TODO Should we remove it?
                    }
                }
            }
        }
        std::vector<std::vector<uint32_t>> rbsPercSlices = {dedicatedRbPercSlices,
                                                            minRbPerSlicesOnly,
                                                            maxRbPerSlicesOnly};
        NS_ASSERT(std::accumulate(dedicatedRbPercSlices.begin(), dedicatedRbPercSlices.end(), 0) <=
                  100);
        NS_ASSERT(std::accumulate(minRbPercSlices.begin(), minRbPercSlices.end(), 0) <= 100);

        // RAN slicing allocation
        for (int allocProcess = 0; allocProcess < 3; allocProcess++) // 0=dedicated, 1=min, 2=max
        {
            for (uint16_t sliceIdx = 0; sliceIdx < numberSlices; sliceIdx++)
            {
                uint32_t slicesResource =
                    floor(total_resources * rbsPercSlices[allocProcess][sliceIdx] / 100.0);
                fprintf(stderr,
                        "Alloc process: %d, Slice %d: %d, RBs\n",
                        allocProcess,
                        sliceIdx,
                        slicesResource);
                while (slicesResource > 0 && resources > 0)
                {
                    // Round-robin for UEs in the slice
                    GetFirst GetUe;
                    std::stable_sort(ranSliceUeVector[sliceIdx].begin(),
                                     ranSliceUeVector[sliceIdx].end(),
                                     GetUeCompareDlFn());
                    auto schedInfoIt = ranSliceUeVector[sliceIdx].begin();

                    // Ensure fairness: pass over UEs which already has enough resources to transmit
                    while (schedInfoIt != ranSliceUeVector[sliceIdx].end())
                    {
                        uint32_t bufQueueSize = schedInfoIt->second;
                        if (GetUe(*schedInfoIt)->m_dlTbSize >= std::max(bufQueueSize, 10U))
                        {
                            schedInfoIt++;
                        }
                        else
                        {
                            break;
                        }
                    }

                    // In the case that all the slice's UEs already have their requirements
                    // fulfilled, then stop the slice processing and pass to the next
                    if (schedInfoIt == ranSliceUeVector[sliceIdx].end())
                    {
                        break;
                    }
                    do
                    {
                        // Assign 1 RBG for each available symbols for the beam,
                        // and then update the count of available resources
                        GetUe(*schedInfoIt)->m_dlRBG += rbgAssignable;
                        assigned.m_rbg += rbgAssignable;

                        GetUe(*schedInfoIt)->m_dlSym = beamSym;
                        assigned.m_sym = beamSym;

                        slicesResource -=
                            1; // Resources are RBG, so they do not consider the beamSym

                        if (allocProcess !=
                            0) // If dedicated(allocProcess=0), then do not update the resources yet
                            resources -= 1;

                        // Update metrics
                        NS_LOG_DEBUG("Assigned " << rbgAssignable << " DL RBG, spanned over "
                                                 << beamSym << " SYM, to UE "
                                                 << GetUe(*schedInfoIt)->m_rnti);
                        // Following call to AssignedDlResources would update the
                        // TB size in the NrMacSchedulerUeInfo of this particular UE
                        // according the Rank Indicator reported by it. Only one call
                        // to this method is enough even if the UE reported rank indicator 2,
                        // since the number of RBG assigned to both the streams are the same.
                        AssignedDlResources(*schedInfoIt,
                                            FTResources(rbgAssignable, beamSym),
                                            assigned);
                    } while (GetUe(*schedInfoIt)->m_dlTbSize < 10 && slicesResource > 0);

                    // Update metrics for the unsuccessful UEs (who did not get any resource in this
                    // iteration)
                    for (auto& ue : ranSliceUeVector[sliceIdx])
                    {
                        if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                        {
                            NotAssignedDlResources(ue,
                                                   FTResources(rbgAssignable, beamSym),
                                                   assigned);
                        }
                    }
                }
            }
            if (allocProcess == 0)
            { // Reduce all the dedicated resources from the total resources (even if the RBs were
              // not used)
                resources -= ceil(
                    resources *
                    std::accumulate(dedicatedRbPercSlices.begin(), dedicatedRbPercSlices.end(), 0) /
                    100);
            }
        }

        for (uint32_t sliceIdx = 0; sliceIdx < numberSlices; sliceIdx++)
        {
            for (auto& ue : ranSliceUeVector[sliceIdx])
            {
                GetFirst GetUe;
                NS_LOG_INFO("UE " << GetUe(ue)->m_rnti << " DL RBG: " << GetUe(ue)->m_dlRBG
                                  << " DL Sym: " << GetUe(ue)->m_dlSym);
            }
        }
    }

    return symPerBeam;
}

} // namespace ns3
