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

#include "kpm-function-description.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "MeasurementInfo-Action-Item.h"
#include "MeasurementInfo-Action-List.h"
#include "MeasurementTypeName.h"
#include "RIC-EventTriggerStyle-Item.h"
#include "RIC-ReportStyle-Item.h"
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

// Helper to add a MeasurementInfo-Action-Item to a list.
static void AddMeasInfoActionItem(MeausrementInfo_Action_List_t* list, const stf::string& measName){
    auto* item = (MeasurementInfo_Action_Item_t*)calloc(1, sizeof(MeasurementInfo_Action_Item_t));
    OCTET_STRING_fromString(&item->measName, measName.c_str());
    ASN_SEQUENCE_ADD(&list->list, item);
}

// Helper to create and fill a RIC_ReportStyle_Item with its metric list.
static RIC_ReportStyle_Item_t* CreateReportStyleItem(long styleType,
                                                    const std::string& styleName,
                                                    long actionFormatType,
                                                    long indicationHearderFormatType,
                                                    long indicationMessageFormatType,
                                                    const std::vector<std::string>& measNames)
{
    auto* style = (RIC_ReportStyle_Item_t*)calloc(1, sizeof(RIC_ReportStyle_Item_t));
    style->ric_ReportStyle_Type = styleType;
    OCTET_STRING_fromString(&style->ric_ReportStyle_Name, styleName.c_str());

    style->ric_ActionFormat_Type = actionFormatType;
    style->ric_IndicationHeaderFormat_Type = indicationHearderFormatType;
    style->ric_IndicationMessageFormat_Type = indicationMessageFormatType;

    for (const auto& name : measNames){
        AddMeasInfoActionItem(&style->measInfo_Action_List, name);
    }
    return style;
}

void
KpmFunctionDescription::FillAndEncodeKpmFunctionDescription(E2SM_KPM_RANfunction_Description_t* ranfunc_desc)
{
    const std::string shortName = "ORAN-E2SM-KPM";
    const std::string description = "KPM Service Model v3.00";
    const std::string oid = "1.3.6.1.4.1.53148.1.3.2.2";

    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_ShortName, shortName.c_str());
    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_Description, description.c_str());
    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_OID, oid.c_str());

    long* inst = (long*)calloc(1, sizeof(long));
    *inst = 0;
    ranfunc_desc->ranFunction_Name.ranFunction_Instance = inst;

    // RIC Event Trigger Style List
    ranfunc_desc->ric_EventTriggerStyle_List_List = (E2SM_KPMfunction_Description::
        E2SM_KPM_RANfunction_Description__ric_EventTriggerStyle_List*)
        calloc(1, sizeof(E2SM_KPMfunction_Description::E2SM_KPM_RANfunction_Description__ric_EventTriggerStyle_List));

    auto* trigger_style = (RIC_EventTriggerStyle_Item_t*)calloc(1, sizeof(RIC_EventTriggerStyle_Item_t));
    trigger_style->ric_EventTriggerStyle_Type = 1;
    OCTET_STRING_fromString(&trigger_style->ric_EventTriggerStyle_Description, "Periodic report");
    trigger_style->ric_EventTriggerFormat_Type = 1;
    ASN_SEQUENCE_ADD(&ranfunc_desc->ric_EventTriggerStyle_List->List, trigger_style);

    // RIC Report Style List
    ranfunc_desc->ric_ReportStyle_List =
        (E2SM_KPM_RANfunction_Description::E2SM_KPM_RANfunction_Description__ric_ReportStyle_List*)
        calloc(1,sizeof(E2SM_KPM_RANfunction_Description::E2SM_KPM_RANfunction_Description__ric_ReportStyle_List));

    std::vector<std::string> style1Metrics = {
        "DRB.PdcpSduVolumeDL",         // Total PDCP SDU volume DL (replaces TB.TotNbrDl.1)
        "DRB.PdcpSduVolumeUL",         // Total PDCP SDU volume UL
        "RRU.PrbUsedDl",               // Used PRBs DL
        "RRU.PrbUsedUl",               // Used PRBs UL
        "RRU.PrbAvailDl",              // Available PRBs DL
        "RRU.PrbAvailUl",              // Available PRBs UL
        "RRU.PrbTotDl",                // Total PRBs DL
        "RRU.PrbTotUl",                // Total PRBs UL
        "DRB.MeanActiveUeDl",          // Mean active UEs DL
        "DRB.MeanActiveUeUl",          // Mean active UEs UL
        "TB.TotNbrDlInitial",          // Total DL initial transmissions
        "TB.TotNbrDlInitial.Qpsk",     // DL initial transmissions QPSK
        "TB.TotNbrDlInitial.16Qam",    // DL initial transmissions 16QAM
        "TB.TotNbrDlInitial.64Qam",    // DL initial transmissions 64QAM
        "RRC.ConnMean",                // Mean RRC connections (replaces numActiveUes)
    };

    RIC_ReportStyle_Item_t* reportStyle1 = CreateReportStyleItem(
        1,                                                       // ric_ReportStyle_Type
        "E2 Node Measurement",                                   // ric_ReportStyle_Name
        1,                                                       // ric_ActionFormat_Type
        1,                                                       //ric_IndicationHeaderFormat_Type
        1,                                                       // ric_IndicationMessageFormat_Type
        style1Metrics
    );

    ASN_SEQUENCE_ADD(&ranfunc_desc->ric_ReportStyle_List->List, reportStyle1);

    std::vector<std::string> style4Metrics = {
        "DRB.UEThpDl",                        // UE throughput DL
        "DRB.UEThpUl",                        // UE throughput UL
        "QosFlow.PdcpPduVolumeDL_Filter",      // QoS flow PDCP PDU volume DL
        "QosFlow.PdcpPduVolumeUL_Filter",      // QoS flow PDCP PDU volume UL
        "DRB.PdcpPduNbrDl.Qos",               // PDCP PDU number DL per QoS
        "DRB.PdcpPduNbrUl.Qos",               // PDCP PDU number UL per QoS
        "HO.SrcCellQual.RS-SINR",             // Handover source cell SINR
        "L1M.RS-SINR",                         // L1 measurement RS-SINR
        "DRB.BufferSize.Qos",                  // Buffer size per QoS
        "DRB.NetworkSlicing.SST",              // Network slicing SST (custom)
    };

    RIC_ReportStyle_Item_t* reportStyle4 = CreateReportStyleItem(
        4,                                                       // ric_ReportStyle_Type
        "Common Condition-based, UE-level E2 Node Measurement",  // ric_ReportStyle_Name
        4,                                                       // ric_ActionFormat_Type
        1,                                                       // ric_IndicationHeaderFormat_Type
        3,                                                       // ric_IndicationMessageFormat_Type (Format 3 = UE level)
        style4Metrics
    );

    ASN_SEQUENCE_ADD(&ranfunc_desc->ric_ReportStyle_List->List, reportStyle4);

    Encode(ranfunc_desc);

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_RANfunction_Description, ranfunc_desc));
}

} // namespace ns3
