// Copyright (c) 2025 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-mac-scheduler-ue-info-rl.h"

#include "ns3/nori-slicing-helper.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrMacSchedulerUeInfoRl");

NrMacSchedulerUeInfoRl::NrMacSchedulerUeInfoRl(uint16_t rnti,
                                               BeamId beamId,
                                               const GetRbPerRbgFn& fn)
    : NrMacSchedulerUeInfo(rnti, beamId, fn)
{
}

uint8_t
NrMacSchedulerUeInfoRl::GetSst() const
{
    uint8_t sst = NoriSlicingHelper::GetSstForRnti(m_rnti);

    // Evidence/log: show deterministic mapping RNTI -> SST at lookup time.
    NS_LOG_INFO("[NrMacSchedulerUeInfoRl] RNTI="
                << m_rnti << " -> SST=" << static_cast<uint32_t>(sst));

    return sst;
}

uint8_t
NrMacSchedulerUeInfoRl::GetSstFromUe(const UePtr& ue)
{
    auto rlPtr = std::dynamic_pointer_cast<NrMacSchedulerUeInfoRl>(ue);
    if (rlPtr)
    {
        return rlPtr->GetSst();
    }

    // Fallback path: UE was created with a different representation type.
    // Still answer using the global RNTI->SST map, without scanning lists.
    uint8_t sst = NoriSlicingHelper::GetSstForRnti(ue->m_rnti);
    NS_LOG_INFO("[NrMacSchedulerUeInfoRl] (fallback) RNTI="
                << ue->m_rnti << " -> SST=" << static_cast<uint32_t>(sst));
    return sst;
}

} // namespace ns3
