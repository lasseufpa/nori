// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ns3/nr-mac-scheduler-ofdma-rr.h"
#include "ns3/nr-mac-scheduler-ofdma.h"
#include "ns3/traced-value.h"

namespace ns3
{

struct SlicePRBQuota
{
    uint32_t sliceId = 0;
    long maxPRBRatio = 0;
    long minPRBRatio = 0;
    long dedicatePRBRatio = 0;
    std::string resourceType = "PRB_DL"; // New: "PRB_DL", "PRB_UL"
    uint32_t maxMimoLayers = 0;

};

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
    void SetSlicingParameters(const std::vector<SlicePRBQuota>& quotas);
    
    void SetSliceUeMapping(uint32_t numSlices, const std::vector<std::vector<uint32_t>>& sliceUeRnti);
    void SetTxLayersChoice(uint8_t choice) { m_txLayersChoice = choice; }
    /**
* @brief Returns the current PRB quotas per slice.
* Used by the CCC (CccIndicationMessage) layer to populate the
* partitionList field in the indications sent to the Near-RT RIC.

* @return Vector of SlicePRBQuota with the current state of each slice.
     */
    std::vector<SlicePRBQuota> GetCurrentSliceQuotas() const;
    void SetMaxMimoLayers(uint8_t layers) { m_maxMimoLayers = layers; }
  
  protected:
    /**
     * @brief Create an UE representation aware of RAN slicing (SST lookup).
     *
     * This overrides the RR default and instantiates NrMacSchedulerUeInfoRl
     * so that, given an RNTI, the scheduler (and E2/KPM) can deterministically
     * obtain the associated SST at runtime without scanning slice lists.
     */
    std::shared_ptr<NrMacSchedulerUeInfo> CreateUeRepresentation(
        const NrMacCschedSapProvider::CschedUeConfigReqParameters& params) const override;

    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;

  private:
    uint32_t m_numberSlices; //!< Number of slices
    std::vector<uint32_t> m_dedicatedRbPercSlices; //!< Dedicated RB percentage per slice
    std::vector<uint32_t> m_minRbPercSlices; //!< Minimum RB percentage per slice
    std::vector<uint32_t> m_maxRbPercSlices; //!< Maximum RB percentage per slice
    std::vector<std::vector<uint32_t>> m_sliceUeRnti; //!< UE RNTI per slice

    // std::vector<uint32_t> m_maxMimoLayersPerSlice;
    uint8_t m_maxMimoLayers{4}; //!< Rank cap applied on gNB side (set by xApp) 
    uint8_t m_txLayersChoice{UINT8_MAX};

    TracedValue<uint32_t> m_tracedValueSymPerBeam;
    void EnforceTxLayers(const std::shared_ptr<NrMacSchedulerUeInfo>& ue) const;
};
} // namespace ns3
