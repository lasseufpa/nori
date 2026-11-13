// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include "ns3/nr-mac-scheduler-ofdma-rr.h"
#include "ns3/nr-mac-scheduler-ofdma.h"
#include "ns3/traced-value.h"

#include <set>

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

    /**
* @brief Returns the current PRB quotas per slice.
* Used by the CCC (CccIndicationMessage) layer to populate the
* partitionList field in the indications sent to the Near-RT RIC.

* @return Vector of SlicePRBQuota with the current state of each slice.
     */
    std::vector<SlicePRBQuota> GetCurrentSliceQuotas() const;

  
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

    /**
     * @brief Assign the DL RBGs of each beam to the UEs, slice by slice.
     *
     * The RBGs of the beam are split in three phases, in this order, as percentages of the
     * RBGs available in the beam (GetDlBitmask()):
     *  - dedicated: each slice reserves its own RBGs; unused ones stay idle;
     *  - min - dedicated: each slice takes up to this share from the common pool;
     *  - max - min: each slice takes up to this share from what is left in the pool.
     * Inside a slice, UEs are served in the round-robin order of NrMacSchedulerOfdmaRR.
     * Without slices configured (SetSliceUeMapping() not called), the RR allocation is used.
     */
    BeamSymbolMap AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const override;

  private:
    /**
     * @brief Allocate RBGs of a candidate set to the UEs of a slice.
     *
     * RBGs are taken in increasing index order (wideband allocation). A UE keeps receiving
     * RBGs until its TB reaches the minimum DCI size (10 bytes); UEs that do not reach it
     * when the budget or the set runs out give their RBGs back to the set.
     *
     * @param ueVector UEs of the slice in this beam, with their buffer
     * @param candidates RBG indices usable by the slice; allocated ones are removed
     * @param budget maximum number of RBGs to allocate
     * @param beamSym symbols of the beam
     * @param assigned resources assigned in the beam (updated)
     * @return number of RBGs allocated
     */
    uint32_t AllocateSliceRbgs(std::vector<UePtrAndBufferReq>& ueVector,
                               std::set<uint32_t>& candidates,
                               uint32_t budget,
                               uint32_t beamSym,
                               FTResources& assigned) const;

    /// Give one RBG (over beamSym symbols) to a UE
    static void AllocateRbgToUe(const std::shared_ptr<NrMacSchedulerUeInfo>& ue,
                                uint32_t rbg,
                                uint32_t beamSym,
                                FTResources& assigned);

    /// Take back all the DL RBGs of a UE, returning them to the candidate set
    static void ReleaseUeRbgs(const std::shared_ptr<NrMacSchedulerUeInfo>& ue,
                              std::set<uint32_t>& candidates,
                              FTResources& assigned);

    uint32_t m_numberSlices; //!< Number of slices
    std::vector<uint32_t> m_dedicatedRbPercSlices; //!< Dedicated RB percentage per slice
    std::vector<uint32_t> m_minRbPercSlices; //!< Minimum RB percentage per slice
    std::vector<uint32_t> m_maxRbPercSlices; //!< Maximum RB percentage per slice
    std::vector<std::vector<uint32_t>> m_sliceUeRnti; //!< UE RNTI per slice

    std::vector<uint32_t> m_maxMimoLayersPerSlice; //


    TracedValue<uint32_t> m_tracedValueSymPerBeam;
};
} // namespace ns3
