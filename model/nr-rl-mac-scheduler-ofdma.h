// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <ns3/nr-mac-scheduler-ofdma-rr.h>
#include <ns3/nr-mac-scheduler-ofdma.h>
#include <ns3/traced-value.h>

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
     * @brief GetTypeId
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

  protected:
    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;

  private:
    uint32_t numberSlices;
    std::vector<uint32_t> dedicatedRbPercSlices;
    std::vector<uint32_t> minRbPercSlices;
    std::vector<uint32_t> maxRbPercSlices;
    std::vector<std::vector<uint32_t>> sliceUeRnti;
    /**
     * @brief Rescue the controled attributes intendended by the RL xApp.
     * This function will define the maximum, minimum and dedicated resources for each slice.
     * @todo Implement it
     */
    [[maybe_unused]] void RescueXAppValues();

    TracedValue<uint32_t> m_tracedValueSymPerBeam;
};
} // namespace ns3
