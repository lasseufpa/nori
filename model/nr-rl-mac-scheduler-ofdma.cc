// Copyright (c) 2025 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-rl-mac-scheduler-ofdma.h"

#include "nr-mac-scheduler-ue-info-rl.h"

#include "ns3/log.h"
#include "ns3/nori-slicing-helper.h"
#include "ns3/nr-fh-control.h"

#include <algorithm>
#include <cmath>
#include <numeric>
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
    // m_maxMimoLayersPerSlice.resize(m_numberSlices, 0);

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

void
NrRLMacSchedulerOfdma::AllocateRbgToUe(const std::shared_ptr<NrMacSchedulerUeInfo>& ue,
                                       uint32_t rbg,
                                       uint32_t beamSym,
                                       FTResources& assigned)
{
    // Same representation as NrMacSchedulerOfdma: one entry per (RBG, symbol) pair
    auto& rbgs = ue->m_dlRBG;
    rbgs.insert(rbgs.end(), beamSym, static_cast<uint16_t>(rbg));
    auto& syms = ue->m_dlSym;
    const auto first = syms.size();
    syms.resize(first + beamSym);
    std::iota(syms.begin() + first, syms.end(), 0);
    assigned.m_rbg++;
    assigned.m_sym = beamSym;
}

void
NrRLMacSchedulerOfdma::ReleaseUeRbgs(const std::shared_ptr<NrMacSchedulerUeInfo>& ue,
                                     std::set<uint32_t>& candidates,
                                     FTResources& assigned)
{
    std::set<uint32_t> released(ue->m_dlRBG.begin(), ue->m_dlRBG.end());
    for (auto rbg : released)
    {
        candidates.insert(rbg);
        NS_ASSERT(assigned.m_rbg > 0);
        assigned.m_rbg--;
    }
    assigned.m_sym = assigned.m_rbg == 0 ? 0 : assigned.m_sym;
    ue->m_dlRBG.clear();
    ue->m_dlSym.clear();
}

uint32_t
NrRLMacSchedulerOfdma::AllocateSliceRbgs(std::vector<UePtrAndBufferReq>& ueVector,
                                         std::set<uint32_t>& candidates,
                                         uint32_t budget,
                                         uint32_t beamSym,
                                         FTResources& assigned) const
{
    constexpr uint32_t minDlTbSize = 10; // minimum TB to create a DL DCI (CreateDlDci)
    GetFirst GetUe;
    uint32_t allocated = 0;

    while (budget > 0 && !candidates.empty())
    {
        std::stable_sort(ueVector.begin(), ueVector.end(), GetUeCompareDlFn());

        // Skip UEs whose TB already covers their buffer
        auto it = std::find_if(ueVector.begin(), ueVector.end(), [&](const auto& ue) {
            return GetUe(ue)->m_dlTbSize < std::max(ue.second, minDlTbSize);
        });
        if (it == ueVector.end())
        {
            break; // every UE of the slice is served
        }

        // Give consecutive RBGs to the same UE until its TB reaches the DCI minimum
        do
        {
            const uint32_t rbg = *candidates.begin();
            candidates.erase(candidates.begin());
            AllocateRbgToUe(GetUe(*it), rbg, beamSym, assigned);
            AssignedDlResources(*it, FTResources(beamSym, beamSym), assigned);
            budget--;
            allocated++;
            NS_LOG_DEBUG("Assigned DL RBG " << rbg << " over " << beamSym << " symbols to UE "
                                            << GetUe(*it)->m_rnti);
        } while (GetUe(*it)->m_dlTbSize < minDlTbSize && budget > 0 && !candidates.empty());

        for (auto& ue : ueVector)
        {
            if (GetUe(ue)->m_rnti != GetUe(*it)->m_rnti)
            {
                NotAssignedDlResources(ue, FTResources(beamSym, beamSym), assigned);
            }
        }
    }

    // A UE below the DCI minimum would get its DCI dropped: return its RBGs to the set
    for (auto& ue : ueVector)
    {
        auto ueInfo = GetUe(ue);
        if (!ueInfo->m_dlRBG.empty() && ueInfo->m_dlTbSize < minDlTbSize)
        {
            const auto rbgs = std::set<uint32_t>(ueInfo->m_dlRBG.begin(), ueInfo->m_dlRBG.end());
            ReleaseUeRbgs(ueInfo, candidates, assigned);
            allocated -= rbgs.size();
            AssignedDlResources(ue, FTResources(beamSym, beamSym), assigned);
            NS_LOG_DEBUG("UE " << ueInfo->m_rnti << " below the minimum TB; released "
                               << rbgs.size() << " RBGs");
        }
    }
    return allocated;
}

NrMacSchedulerNs3::BeamSymbolMap
NrRLMacSchedulerOfdma::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    if (m_numberSlices == 0)
    {
        // No slice mapping configured: plain round-robin OFDMA allocation
        return NrMacSchedulerOfdmaRR::AssignDLRBG(symAvail, activeDl);
    }

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    GetFirst GetUe;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    NS_ASSERT(std::accumulate(m_dedicatedRbPercSlices.begin(), m_dedicatedRbPercSlices.end(), 0U) <=
              100);
    NS_ASSERT(std::accumulate(m_minRbPercSlices.begin(), m_minRbPercSlices.end(), 0U) <= 100);

    for (const auto& el : activeDl)
    {
        const uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        FTResources assigned(0, 0);

        // RBGs usable in this beam (notching applied)
        std::set<uint32_t> pool;
        const std::vector<bool> bitmask = GetDlBitmask();
        for (uint32_t i = 0; i < bitmask.size(); ++i)
        {
            if (bitmask[i])
            {
                pool.insert(i);
            }
        }
        const auto totalRbgs = static_cast<uint32_t>(pool.size());
        NS_ASSERT(totalRbgs > 0);

        // Active UEs of each slice in this beam
        std::vector<std::vector<UePtrAndBufferReq>> sliceUes(m_numberSlices);
        for (const auto& ue : GetUeVector(el))
        {
            for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices; ++sliceIdx)
            {
                const auto& rntis = m_sliceUeRnti[sliceIdx];
                if (std::find(rntis.begin(), rntis.end(), GetUe(ue)->m_rnti) != rntis.end())
                {
                    sliceUes[sliceIdx].emplace_back(ue);
                    BeforeDlSched(sliceUes[sliceIdx].back(), FTResources(beamSym, beamSym));
                    break;
                }
            }
        }

        auto share = [totalRbgs](uint32_t perc) {
            return static_cast<uint32_t>(std::floor(totalRbgs * perc / 100.0));
        };

        // Phase 1: dedicated RBGs, reserved per slice even when unused
        for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices && !pool.empty(); ++sliceIdx)
        {
            std::set<uint32_t> reserved;
            const uint32_t n = std::min<uint32_t>(share(m_dedicatedRbPercSlices[sliceIdx]),
                                                  pool.size());
            for (uint32_t k = 0; k < n; ++k)
            {
                reserved.insert(*pool.begin());
                pool.erase(pool.begin());
            }
            AllocateSliceRbgs(sliceUes[sliceIdx], reserved, n, beamSym, assigned);
        }
        // As in the previous implementation, the dedicated share of the whole cell is
        // ceil(total x sum(dedicated) / 100): RBGs left by the per-slice floor() stay idle,
        // so 100% dedicated in total leaves nothing to the common pool.
        const auto dedicatedSum =
            std::accumulate(m_dedicatedRbPercSlices.begin(), m_dedicatedRbPercSlices.end(), 0U);
        const auto dedicatedTotal =
            static_cast<uint32_t>(std::ceil(totalRbgs * dedicatedSum / 100.0));
        while (!pool.empty() && totalRbgs - pool.size() < dedicatedTotal)
        {
            pool.erase(pool.begin());
        }

        // Phase 2 (min - dedicated) and phase 3 (max - min), from the common pool
        for (int phase = 0; phase < 2; ++phase)
        {
            for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices && !pool.empty(); ++sliceIdx)
            {
                const uint32_t perc =
                    phase == 0 ? m_minRbPercSlices[sliceIdx] - m_dedicatedRbPercSlices[sliceIdx]
                               : m_maxRbPercSlices[sliceIdx] - m_minRbPercSlices[sliceIdx];
                AllocateSliceRbgs(sliceUes[sliceIdx], pool, share(perc), beamSym, assigned);
            }
        }

        for (uint32_t sliceIdx = 0; sliceIdx < m_numberSlices; ++sliceIdx)
        {
            for (const auto& ue : sliceUes[sliceIdx])
            {
                NS_LOG_INFO("Beam " << GetBeamId(el) << " slice " << sliceIdx << " UE "
                                    << GetUe(ue)->m_rnti << " buf=" << ue.second << " RBG="
                                    << std::set<uint16_t>(GetUe(ue)->m_dlRBG.begin(),
                                                          GetUe(ue)->m_dlRBG.end())
                                           .size()
                                    << " TB=" << GetUe(ue)->m_dlTbSize);
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
        // if (m_maxMimoLayersPerSlice.size() < m_numberSlices) {
        //     m_maxMimoLayersPerSlice.resize(m_numberSlices, 0);
        // }
        // m_maxMimoLayersPerSlice[sliceIdx] = q.maxMimoLayers; // 1 ou 2
    }
}

std::vector<SlicePRBQuota>
NrRLMacSchedulerOfdma::GetCurrentSliceQuotas() const
{
    std::vector<SlicePRBQuota> quotas;
    quotas.reserve(m_numberSlices);

    for (uint32_t idx = 0; idx < m_numberSlices; ++idx)
    {
        uint8_t sst = 0;
        if (idx < m_sliceUeRnti.size() && !m_sliceUeRnti[idx].empty())
        {
            uint16_t rnti = static_cast<uint16_t>(m_sliceUeRnti[idx][0]);
            sst = NoriSlicingHelper::GetSstForRnti(rnti);
        }

        SlicePRBQuota q;
        q.sliceId          = (sst > 0) ? sst : static_cast<uint32_t>(idx + 1);
        q.dedicatePRBRatio = (idx < m_dedicatedRbPercSlices.size())
                                 ? static_cast<long>(m_dedicatedRbPercSlices[idx]) : 0L;
        q.minPRBRatio      = (idx < m_minRbPercSlices.size())
                                 ? static_cast<long>(m_minRbPercSlices[idx]) : 0L;
        q.maxPRBRatio      = (idx < m_maxRbPercSlices.size())
                                 ? static_cast<long>(m_maxRbPercSlices[idx]) : 100L;
        quotas.push_back(q);

        NS_LOG_DEBUG("[NrRLMacSchedulerOfdma::GetCurrentSliceQuotas] "
                     "sliceIdx=" << idx
                     << " SST=" << static_cast<uint32_t>(sst)
                     << " ded=" << q.dedicatePRBRatio
                     << "% min=" << q.minPRBRatio
                     << "% max=" << q.maxPRBRatio << "%");
    }
    return quotas;
}

} // namespace ns3