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

#include "kpm-indication.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{

#include "E2SM-KPM-IndicationHeader-Format1.h"
#include "E2SM-KPM-IndicationMessage-Format1.h"
#include "MeasurementInfoList.h"
#include "RIC-EventTriggerStyle-Item.h"
#include "RIC-ReportStyle-Item.h"
#include "TimeStamp.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("KpmIndication");

KpmIndicationHeader::KpmIndicationHeader(KpmRicIndicationHeaderValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationHeader_t;
    FillAndEncodeKpmRicIndicationHeader(descriptor, values);
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
                                                     sizeof(E2SM_KPM_IndicationHeader_Format1_t)); //Header Format 1


    NS_LOG_DEBUG("Timestamp received: " << values.m_timestamp);
    long bigEndianTimestamp = htobe64(values.m_timestamp);
    NS_LOG_DEBUG("Timestamp inverted: " << bigEndianTimestamp);

    Ptr<OctetString> ts = Create<OctetString>((void*)&bigEndianTimestamp, TIMESTAMP_LIMIT_SIZE);

    ind_header->colletStartTime = ts->GetValue(); // receive the collectionStartTime

    // This function describes a optional value  
    //if (!values.m_senderName.empty())
    // {
    //     Ptr<OctetString> sdr_Name = Create<OctetString>(values.m_senderName);

    //     ind_header->senderName = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));

    // 
    //     OCTET_STRING_fromBuf(ind_header->senderName,
    //                          (const char*)sdr_Name->GetValue().buf,
    //                          sdr_Name->GetValue().size);
    // }

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header));

    descriptor->indicationHeader_formats.present = E2SM_KPM_IndicationHeader__indicationHeader_formats_PR_indicationHeader_Format1;
    descriptor->indicationHeader_formats.choice.indicationHeader_Format1 = ind_header;

    Encode(descriptor);
    //ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header);
}

KpmIndicationMessage::KpmIndicationMessage(KpmIndicationMessageValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationMessage_t();
    //CheckConstraints(values); Empty Class
    FillAndEncodeKpmIndicationMessage(descriptor, values);
    delete descriptor;
}

KpmIndicationMessage::~KpmIndicationMessage()
{
    free(m_buffer);
    m_size = 0;
}

// void
// KpmIndicationMessage::CheckConstraints(KpmIndicationMessageValues values)
// {
//     // TODO remove?
//     // if (values.m_crnti.length () != 2)
//     //   {
//     //     NS_FATAL_ERROR ("C-RNTI should have length 2");
//     //   }
//     // if (values.m_plmId.length () != 3)
//     //   {
//     //     NS_FATAL_ERROR ("PLMID should have length 3");
//     //   }
//     // if (values.m_nrCellId.length () != 5)
//     //   {
//     //     NS_FATAL_ERROR ("NR Cell ID should have length 5");
//     //   }
//     // TODO add other constraints
// }
//Empty structure

void
KpmIndicationMessage::Encode(E2SM_KPM_IndicationMessage_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = 0; // disable stack bounds checking
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




void
KpmIndicationMessage::FillAndEncodeKpmIndicationMessage(E2SM_KPM_IndicationMessage_t* descriptor,
                                                        KpmIndicationMessageValues values)
{

    descriptor->indicationMessage_formats.present = E2SM_KPM_IndicationMessage__indicationMessage_formats_PR_indicationMessage_Format1; //Define the format 1

    auto* format1 = (E2SM_KPM_IndicationMessage_Format1_t*)calloc(1, sizeof(E2SM_KPM_IndicationMessage_Format1_t));
    descriptor->indicationMessage_formats.choice.indicationMessage_Format1 = format1;

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationMessage_Format1, format1));

    FillAndEncodeIndicationMessageFormat1(format1, values); 

    // xer_fprint (stderr, &asn_DEF_PF_Container, ranContainer);
    Encode(descriptor);

    // free (ranContainer);
    //ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationMessage_Format1, format);

}

void
KpmIndicationMessage::FillAndEncodeIndicationMessageFormat1(E2SM_KPM_IndicationMessage_Format1* format1, KpmIndicationMessageValues values)
{

//Meas Data values
    for (const auto& recordValue : values.m_measData) {
        Ptr<MeasurementDataItemWrap> dataItemWrap = Create<MeasurementDataItemWrap>(recordValue.m_values);
        
        if (dataItemWrap->GetPointer() != nullptr) {

            ASN_SEQUENCE_ADD(&format1->measData->list, dataItemWrap->GetPointer());
        }
    }
//MeasInfoList
    for (const auto& infoValue : values.m_measInfoList) {


        Ptr<MeasurementInfoItemWrap> infoWrap = Create<MeasurementInfoItemWrap>(infoValue.m_measName);

        infoWrap->AddLabel(infoValue.m_labels);

        auto* infoItem = infoWrap->GetPointer(); //call the wrapper asn1c-types

        if (infoItem) {
            ASN_SEQUENCE_ADD(&format1->measInfoList->list, infoItem);
        }
    }

//Granularity Period
//Check if has values of granularity period
    if (values.m_granularityPeriod > 0) {
        format1->granulPeriod = (GranularityPeriod_t*)calloc(1, sizeof(GranularityPeriod_t));
        *format1->granulPeriod = values.m_granularityPeriod; 
    } else {
        format1->granulPeriod = nullptr;
    }
}
}

 // namespace ns3
