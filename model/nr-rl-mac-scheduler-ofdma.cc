// Copyright (c) 2025 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-rl-mac-scheduler-ofdma.h"

#include "nr-mac-scheduler-ue-info-rl.h"

#include "ns3/log.h"
#include "ns3/nori-slicing-helper.h"
#include "ns3/nr-fh-control.h"

#include <algorithm>
#include <random>
#include <sstream>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("NrRLMacSchedulerOfdma");
NS_OBJECT_ENSURE_REGISTERED(NrRLMacSchedulerOfdma);

TypeId
NrRLMacSchedulerOfdma::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrRLMacSchedulerOfdma")
                            .SetParent<NrMacSchedulerOfdmaRR>()
                            .AddConstructor<NrRLMacSchedulerOfdma>()
        //.AddAttribute("NumberSlices",
        //              "Number of slices",
        //              UintegerValue(1),
        //              MakeUintegerAccessor(&NrRLMacSchedulerOfdma::m_numberSlices),
        //              MakeUintegerChecker<uint32_t>())
        //.AddAttribute("DedicatedRbPercSlices",
        //              "Dedicated RB percentage per slice",
        //              VectorValue(Vector(0.0, 0.0, 0.0)), // ignored initial value.
        //              MakeVectorAccessor(&NrRLMacSchedulerOfdma::m_dedicatedRbPercSlices),
        //              MakeVectorChecker())
        //.AddAttribute("MinRbPercSlices",
        //              "Minimum RB percentage per slice",
        //              Vector2DValue(),
        //              MakeUintegerAccessor(&NrRLMacSchedulerOfdma::m_minRbPercSlices),
        //              MakeUintegerChecker<uint32_t>())
        //.AddAttribute("MaxRbPercSlices",
        //              "Maximum RB percentage per slice",
        //              Vector2DValue(),
        //              MakeUintegerAccessor(&NrRLMacSchedulerOfdma::m_maxRbPercSlices),
        //              MakeUintegerChecker<uint32_t>())
        //.AddAttribute("SliceUeRnti",
        //              "UE RNTI per slice",
        //              Vector2DValue(),
        //              MakeUintegerAccessor(&NrRLMacSchedulerOfdma::m_sliceUeRnti),
        //              MakeUintegerChecker<std::vector<uint32_t>>())
        ;
    return tid;
}

NrRLMacSchedulerOfdma::NrRLMacSchedulerOfdma()
    : NrMacSchedulerOfdmaRR()
{
    NS_LOG_FUNCTION(this);
    // Default values -> SHouldn't be hardcoded
    m_numberSlices = 0;
    // m_minRbPercSlices = {70, 30};
    // m_dedicatedRbPercSlices = {30, 30};
    // m_maxRbPercSlices = {100, 100};
    // m_sliceUeRnti = {{1, 2}, {3, 4}}; //TODO: add automatic population of this structure
}

std::shared_ptr<NrMacSchedulerUeInfo>
NrRLMacSchedulerOfdma::CreateUeRepresentation(
    const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const
{
    NS_LOG_FUNCTION(this);
    // Use RL-aware UE representation so we can answer RNTI->SST at runtime
    // without scanning slice lists; the actual SST lookup is delegated to
    // NoriSlicingHelper via NrMacSchedulerUeInfoRl.
    return std::make_shared<NrMacSchedulerUeInfoRl>(
        params.m_rnti,
        params.m_beamId,
        std::bind(&NrRLMacSchedulerOfdma::GetNumRbPerRbg, this));
}

void
NrRLMacSchedulerOfdma::SetSliceUeMapping(uint32_t numSlices,
                                         const std::vector<std::vector<uint32_t>>& sliceUeRnti)
{
    NS_LOG_FUNCTION(this);
    m_numberSlices = numSlices;
    m_sliceUeRnti = sliceUeRnti;

    NS_ASSERT_MSG(m_numberSlices > 0, "Number of slices must be greater than 0");

    m_dedicatedRbPercSlices.resize(m_numberSlices, 0);
    m_minRbPercSlices.resize(m_numberSlices, 0);
    m_maxRbPercSlices.resize(m_numberSlices, 100);

    // Debug: logar mapeamento slice -> RNTIs
    for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices; ++sliceIdx)
    {
        std::ostringstream oss;
        oss << "Slice " << sliceIdx << " UE RNTIs:";
        for (uint32_t rnti : m_sliceUeRnti[sliceIdx])
        {
            oss << " " << rnti;
        }
        NS_LOG_INFO(oss.str());
    }
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
    std::vector<uint32_t> minRbPerSlicesOnly(m_minRbPercSlices.size());
    std::vector<uint32_t> maxRbPerSlicesOnly(m_maxRbPercSlices.size());
    std::vector<std::vector<UePtrAndBufferReq>> ranSliceUeVector(m_numberSlices);

    std::vector<uint32_t> dedicatedRbPercSlices = m_dedicatedRbPercSlices;
    std::vector<uint32_t> minRbPercSlices = m_minRbPercSlices;
    std::vector<uint32_t> maxRbPercSlices = m_maxRbPercSlices;

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
        // uint32_t m_numberSlices = 2;

        for (uint16_t sliceIdx = 0; sliceIdx < m_numberSlices; sliceIdx++)
        {
            NS_ASSERT(dedicatedRbPercSlices[sliceIdx] <= minRbPercSlices[sliceIdx]);
            NS_ASSERT(minRbPercSlices[sliceIdx] <= maxRbPercSlices[sliceIdx]);
            minRbPerSlicesOnly[sliceIdx] =
                minRbPercSlices[sliceIdx] - dedicatedRbPercSlices[sliceIdx];
            maxRbPerSlicesOnly[sliceIdx] = maxRbPercSlices[sliceIdx] - minRbPercSlices[sliceIdx];

            for (const auto& ue : GetUeVector(el))
            {
                for (uint16_t rnti : m_sliceUeRnti[sliceIdx])
                {
                    if (ue.first->m_rnti == rnti)
                    {
                        ranSliceUeVector[sliceIdx].emplace_back(ue);
                        // Evidence/log: when we associate an active UE to a slice
                        // in the scheduler, also log its SST as seen via the
                        // RNTI->SST mapping (deterministic, no list scan here).
                        uint8_t sst = NrMacSchedulerUeInfoRl::GetSstFromUe(ue.first);
                        NS_LOG_INFO("[NrRLMacSchedulerOfdma] UE RNTI="
                                    << ue.first->m_rnti << " mapped to slice " << sliceIdx
                                    << " with SST=" << static_cast<uint32_t>(sst));
                        BeforeDlSched(ue, FTResources(rbgAssignable, beamSym));
                    }
                }
            }
        }

        // Debug: logar quais UEs ativos foram associados a cada slice neste beam
        for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices; ++sliceIdx)
        {
            std::ostringstream oss;
            oss << "Beam " << GetBeamId(el) << " slice " << sliceIdx << " active UEs:";
            for (const auto& ue : ranSliceUeVector[sliceIdx])
            {
                GetFirst GetUe;
                oss << " RNTI=" << GetUe(ue)->m_rnti << " buf=" << ue.second;
            }
            NS_LOG_INFO(oss.str());
        }
        std::vector<std::vector<uint32_t>> rbsPercSlices = {dedicatedRbPercSlices,
                                                            minRbPerSlicesOnly,
                                                            maxRbPerSlicesOnly};

        NS_LOG_INFO("dedicatedRbPercSlices sum: " << std::accumulate(dedicatedRbPercSlices.begin(), dedicatedRbPercSlices.end(), 0) 
                  << ", minRbPercSlices sum: " << std::accumulate(minRbPercSlices.begin(), minRbPercSlices.end(), 0)
                  << ", maxRbPercSlices sum: " << std::accumulate(maxRbPercSlices.begin(), maxRbPercSlices.end(), 0));

        NS_ASSERT(std::accumulate(dedicatedRbPercSlices.begin(), dedicatedRbPercSlices.end(), 0) <=
                  100);
        NS_ASSERT(std::accumulate(minRbPercSlices.begin(), minRbPercSlices.end(), 0) <= 100);

        // RAN slicing allocation
        for (int allocProcess = 0; allocProcess < 3; allocProcess++) // 0=dedicated, 1=min, 2=max
        {
            for (uint16_t sliceIdx = 0; sliceIdx < m_numberSlices; sliceIdx++)
            {
                uint32_t slicesResource =
                    floor(total_resources * rbsPercSlices[allocProcess][sliceIdx] / 100.0);
                NS_LOG_DEBUG("Alloc process: " << allocProcess << ", Slice " << sliceIdx << ": "
                                               << slicesResource << " RBs");
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

        for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices; sliceIdx++)
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

void
NrRLMacSchedulerOfdma::SetSlicingParameters(
    const std::vector<SlicePRBQuota>& quotas)
{
    NS_LOG_FUNCTION(this);

    // Build a mapping SST (sliceId from RC, e.g., 1/2) -> internal
    // slice index used by the scheduler (0-based), using the
    // RNTI->SST mapping provided by NoriSlicingHelper and the
    // per-slice UE lists configured at attach time.
    std::map<uint8_t, uint32_t> sstToSliceIdx;

    for (uint32_t sliceIdx = 0; sliceIdx < m_sliceUeRnti.size(); ++sliceIdx)
    {
        for (uint32_t rnti32 : m_sliceUeRnti[sliceIdx])
        {
            uint16_t rnti = static_cast<uint16_t>(rnti32);
            uint8_t sst = NoriSlicingHelper::GetSstForRnti(rnti);

            if (sst == 0)
            {
                continue; // unknown / not mapped
            }

            auto it = sstToSliceIdx.find(sst);
            if (it == sstToSliceIdx.end())
            {
                sstToSliceIdx[sst] = sliceIdx;
                NS_LOG_INFO("[NrRLMacSchedulerOfdma] Map SST="
                            << static_cast<uint32_t>(sst)
                            << " -> internal sliceIdx=" << sliceIdx);
            }
            else if (it->second != sliceIdx)
            {
                NS_LOG_WARN("[NrRLMacSchedulerOfdma] SST="
                            << static_cast<uint32_t>(sst)
                            << " appears in multiple slice indices (" << it->second << ","
                            << sliceIdx
                            << "); using the first one for quota mapping.");
            }
        }
    }

    for (const auto& q : quotas)
    {
        uint8_t sst = static_cast<uint8_t>(q.sliceId);
        uint32_t sliceIdx = 0;
        auto it = sstToSliceIdx.find(sst);
        if (it != sstToSliceIdx.end())
        {
            sliceIdx = it->second;
        }
        else if (sst >= 1 && (sst - 1) < m_numberSlices)
        {
            sliceIdx = sst - 1;
        }
        else if (sst < m_numberSlices)
        {
            sliceIdx = sst;
        }
        else
        {
            NS_LOG_WARN("[NrRLMacSchedulerOfdma] Received quota for SST="
                        << static_cast<uint32_t>(sst)
                        << " but no matching slice index exists; ignoring.");
            continue;
        }

        auto dedicated = static_cast<uint32_t>(q.dedicatePRBRatio);
        auto minPRB = static_cast<uint32_t>(q.minPRBRatio);
        auto maxPRB = static_cast<uint32_t>(q.maxPRBRatio);

        if (minPRB < dedicated)
        {
            minPRB = dedicated;
        }
        if (maxPRB < minPRB)
        {
            maxPRB = minPRB;
        }

        //std::cout << "Setting slicing parameters for SST " << static_cast<uint32_t>(sst)
        //          << " (internal sliceIdx=" << sliceIdx << "): " << dedicated
        //          << "% dedicated, " << minPRB << "% min, " << maxPRB
        //          << "% max" << std::endl;

        m_dedicatedRbPercSlices[sliceIdx] = dedicated;
        m_minRbPercSlices[sliceIdx] = minPRB;
        m_maxRbPercSlices[sliceIdx] = maxPRB;
    }
}

} // namespace ns3