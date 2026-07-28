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

#include "kpm-indication.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "E2SM-KPM-IndicationHeader-Format1.h"
#include "E2SM-KPM-IndicationMessage-Format1.h"
#include "TimeStamp.h"
#include "E2SM-KPM-IndicationMessage-Format3.h"
#include "GranularityPeriod.h"
#include "LabelInfoItem.h"
#include "LabelInfoList.h"
#include "MeasurementData.h"
#include "MeasurementDataItem.h"
#include "MeasurementInfoItem.h"
#include "MeasurementInfoList.h"
#include "MeasurementLabel.h"
#include "MeasurementRecord.h"
#include "MeasurementRecordItem.h"
#include "MeasurementType.h"
#include "MeasurementTypeName.h"
#include "UEMeasurementReportItem.h"
#include "UEMeasurementReportList.h"
#include "UEID.h"
#include "UEID-GNB.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("KpmIndication");

KpmIndicationHeader::KpmIndicationHeader(KpmRicIndicationHeaderValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationHeader_t;
    FillAndEncodeKpmRicIndicationHeader(descriptor, values);
    delete descriptor;
}

KpmIndicationHeader::~KpmIndicationHeader()
{
    NS_LOG_FUNCTION(this);
    free(m_buffer);
    m_size = 0;
}

void
KpmIndicationHeader::Encode(E2SM_KPM_IndicationHeader_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = nullptr; // disable stack bounds checking
    asn_encode_to_new_buffer_result_s encodedHeader =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_KPM_IndicationHeader,
                                 descriptor);

    if (encodedHeader.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the RIC Indication Header, errno: "
                       << strerror(errno) << ", failed_type "
                       << encodedHeader.result.failed_type->name << ", structure_ptr "
                       << encodedHeader.result.structure_ptr);
    }

    m_buffer = encodedHeader.buffer;
    m_size = encodedHeader.result.encoded;
}

void
KpmIndicationHeader::FillAndEncodeKpmRicIndicationHeader(E2SM_KPM_IndicationHeader_t* descriptor,
                                                         KpmRicIndicationHeaderValues values)
{
    auto* ind_header =
        (E2SM_KPM_IndicationHeader_Format1_t*)calloc(1,
                                                     sizeof(E2SM_KPM_IndicationHeader_Format1_t));

    NS_LOG_DEBUG("Timestamp received: " << values.m_timestamp);
    long bigEndianTimestamp = htobe64(values.m_timestamp);
    NS_LOG_DEBUG("Timestamp inverted: " << bigEndianTimestamp);

    Ptr<OctetString> ts = Create<OctetString>((void*)&bigEndianTimestamp, TIMESTAMP_LIMIT_SIZE);

    ind_header->colletStartTime = ts->GetValue();

    if (!values.m_fileFormatVersion.empty()){
        ind_header->fileFormatversion = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->fileFormatversion, values.m_fileFormatVersion.c_str());
    }
    if (!values.m_senderName.empty()){
        ind_header->senderName = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->senderName, values.m_senderName.c_str());
    }
    if (!values.m_senderType.empty()){
        ind_header->senderType = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->senderType, values.m_senderType.c_str());
    }
    if (!values.m_vendorName.empty()){
        ind_header->vendorName = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->vendorName, values.m_vendorName.c_str());
    }

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header));

    descriptor->indicationHeader_formats.present = E2SM_KPM_IndicationHeader__indicationHeader_formats_PR_indicationHeader_Format1;
    descriptor->indicationHeader_formats.choice.indicationHeader_Format1 = ind_header;

    Encode(descriptor);
    ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header);
}

// ================ KpmIndicationMessage ================

MeasurementRecordItem_t* CreateMeasurementRecordItemInteger(unsigned long value){
    auto* item = (MeasurementRecordItem_t*)calloc(1, sizeof(MeasurementRecordItem_t));
    item->present = MeasurementRecordItem_PR_integer;
    item->choice.integer = value;
    return item;
}

MeasurementRecordItem_t* CreateMeasurementRecordItemReal(double value){
    auto* item = (MeasurementRecordItem_t*)calloc(1, sizeof(MeasurementRecordItem_t));
    item->present = MeasurementRecordItem_PR_real;
    item->choice.real = value;
    return item;
}

MeasurementRecordItem_t* CreateMeasurementRecordItemNoValue(){
    auto* item = (MeasurementRecordItem_t*)calloc(1, sizeof(MeasurementRecordItem_t));
    item->present = MeasurementRecordItem_PR_noValue;
    return item;
}

MeasurementInfoItem_t* CreateMeasurementInfoItemName(const std::string& measName){
    auto* infoItem = (MeasurementInfoItem_t*)calloc(1, sizeof(MeasurementInfoItem_t));

    // Set measurement type to measName
    infoItem->measType.present = MeasurementType_PR_measName;
    OCTET_STRING_fromString(&infoItem->measType.choice.measName, measName.c_str());

    // Add a single "noLabel" entry to the labelInfoList (required by ASN.1)
    auto* labelItem = (LabelInfoItem_t*)calloc(1, sizeof(LabelInfoItem_t));
    long* noLabel = (long*)calloc(1, sizeof(long));
    *noLabel = 0; // MeasurementLabel__noLabel_true
    labelItem->measLabel.noLabel = noLabel;
    ASN_SEQUENCE_ADD(&infoItem->labelInfoList.list, labelItem);

    return infoItem;
}

KpmIndicationMessage::KpmIndicationMessage(KpmIndicationMessageValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationMessage_t();
    switch (values.m_format)
    {
    case MessageFormat::FORMAT1:
        FillAndEncodeFormat1(descriptor, values);
        break;
    case MessageFormat::FORMAT3:
        FillAndEncodeFormat3(descriptor, values);
        break;
    default:
        NS_FATAL_ERROR("Unsupported message format: " << static_cast<int>(values.m_format));
    }
    delete descriptor;
}

KpmIndicationMessage::~KpmIndicationMessage()
{
    free(m_buffer);
    m_size = 0;
}

void
KpmIndicationMessage::Encode(E2SM_KPM_IndicationMessage_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = nullptr;
    asn_encode_to_new_buffer_result_s encodedMsg =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_KPM_IndicationMessage,
                                 descriptor);

    if (encodedMsg.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the RIC Indication Message, errno: "
                       << strerror(errno) << ", failed_type " << encodedMsg.result.failed_type->name
                       << ", structure_ptr " << encodedMsg.result.structure_ptr);
    }

    m_buffer = encodedMsg.buffer;
    m_size = encodedMsg.result.encoded;
}

void KpmIndicationMessage::FillAndEncodeFormat1(E2SM_KPM_IndicationMessage_t* descriptor, KpmIndicationMessageValues values){
    auto* format1 = (E2SM_KPM_IndicationMessage_Format1_t*)calloc(1, sizeof(E2SM_KPM_IndicationMessage_Format1_t));
    if (!values.m_measNames.empty()){
        auto* measInfoList = (MeasurementInfoList_t*)calloc(1, sizeof(MeasurementInfoList_t));
        for (const auto& name : values.m_measNames){
            MeasurementInfoItem_t* infoItem = CreateMeasurementInfoItemName(name);
            ASN_SEQUENCE_ADD(&measInfoList->list, infoItem);
        }
        format1->measInfoList = measInfoList;
    }

    auto* dataItem = (MeasurementDataItem_t*)calloc(1, sizeof(MeasurementDataItem_t));
    for (auto* recordItem : values.m_measRecordItems){
        ASN_SEQUENCE_ADD(&dataItem->measRecord.list, recordItem);
    }
    ASN_SEQUENCE_ADD(&format1->measData.list, dataItem);

    if (values.m_granularityPeriod > 0){
        auto* granul = (GranularityPeriod_t*)calloc(1, sizeof(GranularityPeriod_t));
        *granul = values.m_granularityPeriod;
        format1->granulPeriod = granul;
    }

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationMessage_Format1, format1));

    descriptor->indicationMessage_formats.present = E2SM_KPM_IndicationMessage__indicationMessage_formats_PR_indicationMessage_Format1;
    descriptor->indicationMessage_formats.choice.indicationMessage_Format1 = format1;

    Encode(descriptor);
    ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationMessage_Format1, format1);
}

void KpmIndicationMessage::FillAndEncodeFormat3(E2SM_KPM_IndicationMessage_t* descriptor, KpmIndicationMessageValues values){
    auto* format3 = (E2SM_KPM_IndicationMessage_Format3_t*)calloc(1, sizeof(E2SM_KPM_IndicationMessage_Format3_t));
    for (auto& ueReport : values.m_ueReports){
        auto* reportItem = (UEMeasurementReportItem_t*)calloc(1, sizeof(UEMeasurementReportItem_t));

        NS_ABORT_MSG_IF(ueReport.m_ueId == nullptr, "UEID must not be null for format 3.");
        reportItem->ueID = *ueReport.m_ueId;

        if(!ueReport.m_measNames.empty()){
            auto* measInfoList = (MeasurementInfoList_t*)calloc(1, sizeof(MeasurementInfoList_t));
            for (const auto& name : ueReport.m_measNames){
                MeasurementInfoItem_t* infoItem = CreateMeasurementInfoItemName(name);
                ASN_SEQUENCE_ADD(&measInfoList->list, infoItem);
            }
            reportItem->measReport.measInfoList = measInfoList;
        }

        auto* dataItem = (MeasurementDataItem_t*)calloc(1, sizeof(MeasurementDataItem_t));
        for (auto* recordItem : ueReport.m_measRecordItems){
            ASN_SEQUENCE_ADD(&dataItem->measRecord.list, recordItem);
        }
        ASN_SEQUENCE_ADD(&reportItem->measReport.measData.list, dataItem);
        ASN_SEQUENCE_ADD(&format3->ueMeasReportList.list, reportItem);
    }
    
    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationMessage_Format3, format3));

    descriptor->indicationMessage_formats.present = E2SM_KPM_IndicationMessage__indicationMessage_formats_PR_indicationMessage_Format3;
    descriptor->indicationMessage_formats.choice.indicationMessage_Format3 = format3;

    Encode(descriptor);
    ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationMessage_Format3, format3);
}

} // namespace ns3
