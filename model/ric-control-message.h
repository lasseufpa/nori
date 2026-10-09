/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 *         Andrey Adailso <andreyadailsom@gmail.com>
 */

#pragma once

#include "asn1c-types.h"

#include "ns3/object.h"

#include <string>
#include <vector>

extern "C"
{
#include "E2AP-PDU.h"
#include "E2SM-RC-ControlHeader-Format1.h"
#include "E2SM-RC-ControlHeader.h"
#include "E2SM-RC-ControlMessage-Format1.h"
#include "E2SM-RC-ControlMessage.h"
#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"
#include "RANParameter-LIST.h"
#include "RANParameter-STRUCTURE-Item.h"
#include "RANParameter-STRUCTURE.h"
#include "RANParameter-Value.h"
#include "RANParameter-ValueType.h"
#include "RICcontrolRequest.h"
#include "UEID.h"
}

namespace ns3
{

class RicControlMessage : public SimpleRefCount<RicControlMessage>
{
  public:
    /**
     * Structure to store the slice RAN parameters extracted from the RIC Control message
     */
    struct SlicePRBQuota
    {
        uint8_t sst{0};         // Slice/Service Type
        uint32_t sd{0};         // Slice Differentiator (optional)
        long maxPRBRatio{100};  // Maximum PRB policy ratio
        long minPRBRatio{0};    // Minimum PRB policy ratio
        long dedicatePRBRatio{0}; // Dedicated PRB policy ratio
    };

    enum ControlMessageRequestIdType
    {
        TS = 1001,
        QoS = 1002,
        RAN_SLICING = 1003,
    };

    RicControlMessage(E2AP_PDU_t* pdu);
    ~RicControlMessage();

    ControlMessageRequestIdType GetRequestType() const
    {
        return m_requestType;
    }

    const std::vector<SlicePRBQuota>& GetPrbQuotas() const
    {
        return m_prbQuotas;
    }

    uint64_t GetUeId() const
    {
        return m_ueId;
    }

    long GetControlStyleType() const
    {
        return m_ricStyleType;
    }

    long GetControlActionId() const
    {
        return m_ricControlActionId;
    }

    long GetControlDecision() const
    {
        return m_ricControlDecision;
    }

    const RICrequestID_t& GetRicRequestId() const
    {
        return m_ricRequestId;
    }

    RANfunctionID_t GetRanFunctionId() const
    {
        return m_ranFunctionId;
    }

    const RICcallProcessID_t& GetRicCallProcessId() const
    {
        return m_ricCallProcessId;
    }

    std::string GetSecondaryCellIdHO() const;

    ControlMessageRequestIdType m_requestType{RAN_SLICING};
    RANfunctionID_t m_ranFunctionId{0};
    RICrequestID_t m_ricRequestId{0, 0};
    RICcallProcessID_t m_ricCallProcessId{nullptr, 0};
    uint64_t m_ueId{0};
    long m_ricStyleType{0};
    long m_ricControlActionId{0};
    long m_ricControlDecision{0}; // 0 = accept, 1 = reject

    std::vector<SlicePRBQuota> m_prbQuotas;

  private:
    /**
     * Decodes the RIC Control message PDU.
     *
     * @param pdu PDU passed by the RIC
     */
    void DecodeRicControlMessage(E2AP_PDU_t* pdu);
    void DecodeControlHeader(const RICcontrolHeader_t& headerIe);
    void DecodeControlMessage(const RICcontrolMessage_t& messageIe);

    void ParseRanParameterStructureForSlicing(const RANParameter_STRUCTURE_t* structure);
    void ParseRanParameterListForSlicing(const RANParameter_LIST_t* list);
    void ParseRanParameterValueTypeForSlicing(long paramId,
                                             const RANParameter_ValueType_t* valueType);

    std::string m_secondaryCellId;
};

} // namespace ns3
