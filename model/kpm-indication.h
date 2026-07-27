/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 *         João Albuquerque <joao.barbosa.albuquerque@itec.ufpa.br>
 *         Andrey Adailso <andreyadailsom@gmail.com>
 */

#pragma once

#include "ns3/object.h"

#include <set>
#include <string>
#include <vector>

extern "C"
{
#include "E2SM-KPM-IndicationHeader.h"
#include "E2SM-KPM-IndicationHeader-Format1.h"
#include "E2SM-KPM-IndicationMessage.h"
#include "E2SM-KPM-RANfunction-Description.h"
#include "E2SM-KPM-IndicationMessage-Format1.h"
#include "E2SM-KPM-IndicationMessage-Format3.h"
#include "MeasurementData.h"
#include "MeasurementDataItem.h"
#include "MeasurementInfoItem.h"
#include "MeasurementInfoList.h"
#include "MeasurementLabel.h"
#include "MeasurementRecord.h"
#include "MeasurementRecordItem.h"
#include "MeasurementType.h"
#include "MeasurementTypeName.h"
#include "LabelInfoItem.h"
#include "LabelInfoList.h"
#include "GranularityPeriod.h"
#include "UEMeasurementReportItem.h"
#include "UEMeasurementReportList.h"
#include "UEID.h"
#include "UEID-GNB.h"
#include "asn1c-types.h"
}

namespace ns3
{

class KpmIndicationHeader : public SimpleRefCount<KpmIndicationHeader>
{
  public:
    const int TIMESTAMP_LIMIT_SIZE = 8;

    struct KpmRicIndicationHeaderValues
    {
      uint64_t m_timestamp;
      std::string m_fileFormatVersion;
      std::string m_senderName;
      std::string m_senderType;
      std::string m_vendorName;
    };

    KpmIndicationHeader(KpmRicIndicationHeaderValues values);
    ~KpmIndicationHeader();

    void* m_buffer;
    size_t m_size;

  private:
    void FillAndEncodeKpmRicIndicationHeader(E2SM_KPM_IndicationHeader_t* descriptor, KpmRicIndicationHeaderValues values);
    void Encode(E2SM_KPM_IndicationHeader_t* descriptor);
};

MeasurementRecordItem_t* CreateMeasurementRecordItemInteger(unsigned long value);

MeasurementRecordItem_t* CreateMeasurementRecordItemReal(double value);

MeasurementRecordItem_t* CreateMeasurementRecordItemNoValue();

MeasurementInfoItem_t* CreateMeasurementInfoItemName(const std::string& measName);

class KpmIndicationMessage : public SimpleRefCount<KpmIndicationMessage>
{
  public:
    enum class MessageFormat
    {
        FORMAT1 = 1,
        FORMAT3 = 3
    };

    struct UeReportValues
    {
        UEID_t* m_ueId = nullptr;
        std::vector<std::string> m_measNames;
        std::vector<MeasurementRecordItem_t*> m_measRecordItems;
    };

    struct KpmIndicationMessageValues
    {
        MessageFormat m_format = MessageFormat::FORMAT1;

        std::vector<std::string> m_measNames;
        std::vector<MeasurementRecordItem_t*> m_measRecordItems;
        unsigned long m_granularityPeriod = 0;

        std::vector<UeReportValues> m_ueReports;
    };

    KpmIndicationMessage(KpmIndicationMessageValues values);
    ~KpmIndicationMessage();

    void* m_buffer;
    size_t m_size;

  private:
    void FillAndEncodeFormat1(E2SM_KPM_IndicationMessage_t* descriptor, KpmIndicationMessageValues values);
    void FillAndEncodeFormat3(E2SM_KPM_IndicationMessage_t* descriptor, KpmIndicationMessageValues values);
    void Encode(E2SM_KPM_IndicationMessage_t* descriptor);
};

} // namespace ns3
