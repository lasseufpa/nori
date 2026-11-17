/*
 * Copyright (c) 2026 LASSE/UFPA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "ccc-data-models.h"
#include "nr-rl-mac-scheduler-ofdma.h"
#include <vector>

extern "C" {
#include "E2AP-PDU.h"
}

namespace ns3 {

/**
 * @ingroup nori-ccc
 * @brief Decoder of E2SM-CCC RIC Control Requests (O-RRMPolicyRatio and legacy format).
 */
class CccControlMessage {
public:
    explicit CccControlMessage(E2AP_PDU_t* pdu);

    bool IsValid() const { return m_valid; }

    std::vector<SlicePRBQuota> GetPrbQuotas() const;

    int GetRicStyleType() const { return m_ricStyleType; }

    const CccCellDuControlMessage& GetData() const { return m_controlData; }

private:
    bool m_valid{false};
    int  m_ricStyleType{-1};              
    CccCellDuControlMessage m_controlData; 
    std::vector<SlicePRBQuota> m_prbQuotas; 
    uint32_t maxMimoLayers{0};            
};

} // namespace ns3