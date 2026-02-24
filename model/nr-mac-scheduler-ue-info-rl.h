// Copyright (c) 2025 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ns3/nr-mac-scheduler-ue-info.h"

namespace ns3
{

/**
 * \ingroup scheduler
 * \brief UE representation for RL scheduler with slice awareness (SST lookup).
 *
 * This class extends the generic NrMacSchedulerUeInfo with a helper
 * to retrieve, at runtime, the SST associated with the UE RNTI, using
 * the centralized mapping managed by NoriSlicingHelper.
 *
 * The retrieval is O(1) over a map<rnti,sst> and does not depend on any
 * internal per-slice UE list, avoiding ambiguity when answering the
 * question "which SST does this RNTI belong to?".
 */
class NrMacSchedulerUeInfoRl : public NrMacSchedulerUeInfo
{
  public:
    /**
     * \brief Constructor.
     * \param rnti   UE RNTI
     * \param beamId UE beam identifier
     * \param fn     Functor used to retrieve the number of RB per RBG
     */
    NrMacSchedulerUeInfoRl(uint16_t rnti, BeamId beamId, const GetRbPerRbgFn& fn);

    /**
     * \brief Get the SST associated with this UE RNTI.
     *
     * The semantic convention for SST is centralized in NoriSlicingHelper:
     *  - slice index 0 -> SST = 1
     *  - slice index 1 -> SST = 2
     *  - any other slice index or unknown RNTI -> SST = 0 ("no slice" / unknown)
     *
     * This method is deterministic at runtime and scenario-independent.
     *
     * \return SST value in {0,1,2}.
     */
    uint8_t GetSst() const;

    /**
     * \brief Convenience helper to obtain the SST from a generic UePtr.
     *
     * If the underlying UE representation is NrMacSchedulerUeInfoRl, it will
     * use GetSst(); otherwise it will fall back to the global RNTI->SST map.
     * This keeps the lookup O(1) and unambiguous.
     *
     * \param ue Shared pointer to the UE representation.
     * \return SST value in {0,1,2}.
     */
    static uint8_t GetSstFromUe(const UePtr& ue);
};

} // namespace ns3
