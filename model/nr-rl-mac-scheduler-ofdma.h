// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ns3/nr-mac-scheduler-ofdma-rr.h"
#include "ns3/nr-mac-scheduler-ofdma.h"
#include "ns3/ric-control-message.h"
#include "ns3/traced-value.h"

namespace ns3
{

/**
 * @ingroup scheduler
 * @brief Simple RL scheduler for RAN slicing allocation
 *
 * @todo Simple examplanation here
 */
class NrRLMacSchedulerOfdma : public NrMacSchedulerOfdmaRR
{
  public:
    /**
     * @brief GetTypeIdNrRLMacSchedulerOfdma
     * @return The TypeId of the class
     */
    static TypeId GetTypeId();

    /**
     * @brief NrRLMacSchedulerOfdma constructor
     */
    NrRLMacSchedulerOfdma();

    /**
     * @brief Deconstructor
     */
    ~NrRLMacSchedulerOfdma() override
    {
    }

    /**
     * @brief Set the slicing parameters for a specific slice:
     * 
     *  - Dedicated physical resource block per slice
     * 
     *  - Minimum physical resource block per slice
     * 
     *  - Maximum physical resource block per slice
     * 
     * @param slicePRBQuota The slice PRB quota
     */
    void SetSlicingParameters(const std::vector<RicControlMessage::SlicePRBQuota>& quotas);

  protected:
    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;

  private:
    uint32_t m_numberSlices; //!< Number of slices
    std::vector<uint32_t> m_dedicatedRbPercSlices; //!< Dedicated RB percentage per slice
    std::vector<uint32_t> m_minRbPercSlices; //!< Minimum RB percentage per slice
    std::vector<uint32_t> m_maxRbPercSlices; //!< Maximum RB percentage per slice
    std::vector<std::vector<uint32_t>> m_sliceUeRnti; //!< UE RNTI per slice

    TracedValue<uint32_t> m_tracedValueSymPerBeam;
};
} // namespace ns3
