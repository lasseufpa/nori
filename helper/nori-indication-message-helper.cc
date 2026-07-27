/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "nori-indication-message-helper.h"

#include "ns3/log.h"

namespace ns3
{
    
NS_LOG_COMPONENT_DEFINE("NoriIndicationMessageHelper");


NoriIndicationMessageHelper::NoriIndicationMessageHelper(IndicationMessageType type,
                                                         bool isOffline,
                                                         bool reducedPmValues)
    : IndicationMessageHelper(type, isOffline, reducedPmValues)
{
}

NoriIndicationMessageHelper::~NoriIndicationMessageHelper()
{
}

// Format 1 (Node Level)   

void NoriIndicationMessageHelper::SetGranularityPeriod(unsigned long periodMs)
{
    m_msgValues.m_granulPeriod = periodMs;
}

void NoriIndicationMessageHelper::AddNodeMeasurementInteger(const std::string& measName,
                                                       unsigned long value)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::NodeLevel, "AddNodeMeasurement* must be used with NodeLevel type");
    m_msgValues.m_measNames.push_back(measName);
    m_msgValues.m_measRecordItems.push_back(CreateMeasurementRecordItemInteger(value));   
}

void NoriIndicationMessageHelper::AddNodeMeasurementReal(const std::string& measName, double value)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::NodeLevel,
                    "AddNodeMeasurement* must be used with NodeLevel type");
    m_msgValues.m_measNames.push_back(measName);
    m_msgValues.m_measRecordItems.push_back(CreateMeasurementRecordItemReal(value));
}

// Format 3 (UeLevel)

UEID_t* NoriIndicationMessageHelper::BuildGnbUeId(uint64_t amfUeNgapId,
                                           const std::string& plmnId,
                                           uint8_t amfRegionId,
                                           uint16_t amfSetId,
                                           uint8_t amfPointer)
{
    auto* ueid = (UEID_t*)calloc(1, sizeof(UEID_t));
    ueid->present = UEID_PR_gNB_UEID;

    auto* gnbUeid = (UEID_GNB_t*)calloc(1, sizeof(UEID_GNB_t));

    // AMF-UE-NGAP-ID
    asn_long2INTEGER(&gnbUeid->amf_UE_NGAP_ID, (long)amfUeNgapId);

    // GUAMI
    OCTET_STRING_fromString(&gnbUeid->guami.pLMNIdentity, plmnId.c_str());

    // AMF Region ID (8 bits)
    gnbUeid->guami.aMFRegionID.buf = (uint8_t*)calloc(1, 1);
    gnbUeid->guami.aMFRegionID.buf[0] = amfRegionId;
    gnbUeid->guami.aMFRegionID.size = 1;
    gnbUeid->guami.aMFRegionID.bits_unused = 0;

    // AMF Set ID (10 bits)
    gnbUeid->guami.aMFSetID.buf = (uint8_t*)calloc(1, 2);
    gnbUeid->guami.aMFSetID.buf[0] = (amfSetId >> 2) & 0xFF;
    gnbUeid->guami.aMFSetID.buf[1] = (amfSetId << 6) & 0xC0;
    gnbUeid->guami.aMFSetID.size = 2;
    gnbUeid->guami.aMFSetID.bits_unused = 6;

    // AMF Pointer (6 bits)
    gnbUeid->guami.aMFPointer.buf = (uint8_t*)calloc(1, 1);
    gnbUeid->guami.aMFPointer.buf[0] = (amfPointer << 2) & 0xFC;
    gnbUeid->guami.aMFPointer.size = 1;
    gnbUeid->guami.aMFPointer.bits_unused = 2;

    ueid->choice.gNB_UEID = gnbUeid;
    return ueid;
}

void NoriIndicationMessageHelper::BeginUeReport(uint64_t amfUeNgapId,
                                            const std::string& plmnId,
                                            uint8_t amfRegionId,
                                            uint16_t amfSetId,
                                            uint8_t amfPointer)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::UeLevel, "BeginUeReport must be used with UeLevel type");

    KpmIndicationMessage::UeReportValues ueReport;
    ueReport.ueId = BuildGnbUeId(amfUeNgapId, plmnId, amfRegionId, amfSetId, amfPointer);
    m_msgValues.m_ueReports.push_back(ueReport);
}

void NoriIndicationMessageHelper::AddUeMeasurementInteger(const std::string& measName,
                                                     unsigned long value)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::UeLevel, "AddUeMeasurement* must be used with UeLevel type");
    NS_ABORT_MSG_IF(m_msgValues.m_ueReports.empty(), "Call BeginUeReport before adding UE measurements");

    auto& currentUe = m_msgValues.m_ueReports.back();
    currentUe.measNames.push_back(measName);
    currentUe.measRecordItems.push_back(CreateMeasurementRecordItemInteger(value));
}

void NoriIndicationMessageHelper::AddUeMeasurementReal(const std::string& measName, double value)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::UeLevel,
                    "AddUeMeasurement* must be used with UeLevel type");
    NS_ABORT_MSG_IF(m_msgValues.m_ueReports.empty(),
                    "Call BeginUeReport before adding UE measurements");

    auto& currentUe = m_msgValues.m_ueReports.back();
    currentUe.measNames.push_back(measName);
    currentUe.measRecordItems.push_back(CreateMeasurementRecordItemReal(value));
}

}