/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 */

#pragma once

#include "asn1c-types.h"

#include "ns3/object.h"

extern "C"
{
#include "CellGlobalID.h"
#include "E2AP-PDU.h"
#include "E2SM-RC-ControlHeader-Format1.h"
#include "E2SM-RC-ControlHeader.h"
#include "E2SM-RC-ControlMessage-Format1.h"
#include "E2SM-RC-ControlMessage.h"
#include "InitiatingMessage.h"
#include "NRCGI.h"
#include "ProtocolIE-Field.h"
#include "RICcontrolRequest.h"
}

namespace ns3
{

class RicControlMessage : public SimpleRefCount<RicControlMessage>
{
  public:
    enum ControlMessageRequestIdType
    {
        TS = 1001,
        QoS = 1002
    };

    RicControlMessage(E2AP_PDU_t* pdu);
    ~RicControlMessage();

    ControlMessageRequestIdType m_requestType;

    static std::vector<RANParameterItem> ExtractRANParametersFromControlMessage(
        E2SM_RC_ControlMessage_Format1_t* e2SmRcControlMessageFormat1);

    std::vector<RANParameterItem> m_valuesExtracted;
    RANfunctionID_t m_ranFunctionId;
    RICrequestID_t m_ricRequestId;
    RICcallProcessID_t m_ricCallProcessId;
    E2SM_RC_ControlHeader_Format1_t* m_e2SmRcControlHeaderFormat1;
    std::string GetSecondaryCellIdHO();

  private:
    /**
     * Decodes the RIC Control message .
     *
     * @param pdu PDU passed by the RIC
     */
    void DecodeRicControlMessage(E2AP_PDU_t* pdu);
    std::string m_secondaryCellId;
};
} // namespace ns3
