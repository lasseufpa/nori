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

#include "kpm-function-description.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "RIC-EventTriggerStyle-Item.h"
#include "RIC-ReportStyle-Item.h"
#include "MeasurementInfo-Action-Item.h"
#include "RANfunction-Name.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("KpmFunctionDescription");

KpmFunctionDescription::KpmFunctionDescription()
{
    E2SM_KPM_RANfunction_Description_t* descriptor = new E2SM_KPM_RANfunction_Description_t();
    FillAndEncodeKpmFunctionDescription(descriptor);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_RANfunction_Description, descriptor);
    delete descriptor;
}

KpmFunctionDescription::~KpmFunctionDescription()
{
    free(m_buffer);
    m_size = 0;
}

void
KpmFunctionDescription::Encode(E2SM_KPM_RANfunction_Description_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = 0; // disable stack bounds checking
    // encode the structure into the e2smbuffer
    asn_encode_to_new_buffer_result_s encodedMsg =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_KPM_RANfunction_Description,
                                 descriptor);

    if (encodedMsg.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the RIC Indication Header, errno: "
                       << strerror(errno) << ", failed_type " << encodedMsg.result.failed_type->name
                       << ", structure_ptr " << encodedMsg.result.structure_ptr);
    }

    m_buffer = encodedMsg.buffer;
    m_size = encodedMsg.result.encoded;
}

void
KpmFunctionDescription::FillAndEncodeKpmFunctionDescription(
    E2SM_KPM_RANfunction_Description_t* ranfunc_desc)
{
    std::string shortNameBuffer = "ORAN-E2SM-KPM";
    uint8_t* descriptionBuffer = (uint8_t*)"KPM monitor";
    uint8_t* oidBuffer = (uint8_t*)"OID123"; // this is optional, dummy value

    ranfunc_desc->ranFunction_Name = (RANfunction_Name_t*)calloc(1, sizeof(RANfunction_Name_t)); 

    ranfunc_desc->ranFunction_Name->ranFunction_ShortName.size = shortNameBuffer.size();
    ranfunc_desc->ranFunction_Name->ranFunction_ShortName.buf = (uint8_t*)calloc(1, shortNameBuffer.size()+1);
    std::memcpy(ranfunc_desc->ranFunction_Name->ranFunction_ShortName.buf, shortNameBuffer.c_str(), shortNameBuffer.size());

    long* inst = (long*)calloc(1, sizeof(long));

    //  ranfunc_desc->ranFunction_Name.ranFunction_Description = (OCTET_STRING_t*)calloc(1,
    //  sizeof(OCTET_STRING_t));
    ranfunc_desc->ranFunction_Name->ranFunction_Description.buf =
        (uint8_t*)calloc(1, strlen((char*)descriptionBuffer)+1);
    memcpy(ranfunc_desc->ranFunction_Name->ranFunction_Description.buf,
           descriptionBuffer,
           strlen((char*)descriptionBuffer));
    ranfunc_desc->ranFunction_Name->ranFunction_Description.size = strlen((char*)descriptionBuffer);
    ranfunc_desc->ranFunction_Name->ranFunction_Instance = inst;

    //  ranfunc_desc->ranFunction_Name.ranFunction_E2SM_OID = (OCTET_STRING_t*)calloc(1,
    //  sizeof(OCTET_STRING_t));
    ranfunc_desc->ranFunction_Name->ranFunction_E2SM_OID.buf =
        (uint8_t*)calloc(1, strlen((char*)oidBuffer));
    memcpy(ranfunc_desc->ranFunction_Name->ranFunction_E2SM_OID.buf,
           oidBuffer,
           strlen((char*)oidBuffer));
    ranfunc_desc->ranFunction_Name->ranFunction_E2SM_OID.size = strlen((char*)oidBuffer);

    RIC_EventTriggerStyle_Item_t* trigger_style =
        (RIC_EventTriggerStyle_Item_t*)calloc(1, sizeof(RIC_EventTriggerStyle_Item_t));
    trigger_style->ric_EventTriggerStyle_Type = 1;
    uint8_t* eventTriggerStyleNameBuffer = (uint8_t*)"Periodic report";
    //  trigger_style->ric_EventTriggerStyle_Name = (OCTET_STRING_t*)calloc(1,
    //  sizeof(OCTET_STRING_t));
    trigger_style->ric_EventTriggerStyle_Name.buf =
        (uint8_t*)calloc(1, strlen((char*)eventTriggerStyleNameBuffer));
    memcpy(trigger_style->ric_EventTriggerStyle_Name.buf,
           eventTriggerStyleNameBuffer,
           strlen((char*)eventTriggerStyleNameBuffer));
    trigger_style->ric_EventTriggerStyle_Name.size = strlen((char*)eventTriggerStyleNameBuffer);
    trigger_style->ric_EventTriggerFormat_Type = 1;


    ranfunc_desc->ric_EventTriggerStyle_List =
        (E2SM_KPM_RANfunction_Description::
             E2SM_KPM_RANfunction_Description__ric_EventTriggerStyle_List*)
            calloc(1,
                   sizeof(E2SM_KPM_RANfunction_Description::
                              E2SM_KPM_RANfunction_Description__ric_EventTriggerStyle_List));

    ASN_SEQUENCE_ADD(&ranfunc_desc->ric_EventTriggerStyle_List->list, trigger_style);

    RIC_ReportStyle_Item_t* report_style1 =
        (RIC_ReportStyle_Item_t*)calloc(1, sizeof(RIC_ReportStyle_Item_t));
    report_style1->ric_ReportStyle_Type = 1;

    uint8_t* reportStyleNameBuffer =
        (uint8_t*)"E2 Node Measurement";

    //  report_style1->ric_ReportStyle_Name = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    report_style1->ric_ReportStyle_Name.buf =
        (uint8_t*)calloc(1, strlen((char*)reportStyleNameBuffer));
    memcpy(report_style1->ric_ReportStyle_Name.buf,
           reportStyleNameBuffer,
           strlen((char*)reportStyleNameBuffer));
    report_style1->ric_ReportStyle_Name.size = strlen((char*)reportStyleNameBuffer);
    report_style1->ric_ActionFormat_Type = 1; //New
    report_style1->measInfo_Action_List = (MeasurementInfo_Action_List_t*)calloc(1, sizeof(MeasurementInfo_Action_List_t));

    for (size_t i = 0; i < KPM_SUPPORTED_METRICS.size(); ++i) {
        auto* m_item = (MeasurementInfo_Action_Item_t*)calloc(1, sizeof(MeasurementInfo_Action_Item_t));
        
        m_item->measID = (MeasurementTypeID_t*)calloc(1, sizeof(MeasurementTypeID_t));
        *m_item->measID = i + 1;
        
        // Copy metrics name 
        OCTET_STRING_fromBuf(&m_item->measName, 
                             KPM_SUPPORTED_METRICS[i].c_str(), 
                             KPM_SUPPORTED_METRICS[i].length());

        //Update data
        ASN_SEQUENCE_ADD(&report_style1->measInfo_Action_List->list, m_item);
    }



    report_style1->ric_IndicationHeaderFormat_Type = 1;
    report_style1->ric_IndicationMessageFormat_Type = 1;
    ranfunc_desc->ric_ReportStyle_List =
        (E2SM_KPM_RANfunction_Description::E2SM_KPM_RANfunction_Description__ric_ReportStyle_List*)
            calloc(1,
                   sizeof(E2SM_KPM_RANfunction_Description::
                              E2SM_KPM_RANfunction_Description__ric_ReportStyle_List));

    ASN_SEQUENCE_ADD(&ranfunc_desc->ric_ReportStyle_List->list, report_style1);

    Encode(ranfunc_desc);

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_RANfunction_Description, ranfunc_desc));
}
}
// namespace ns3
