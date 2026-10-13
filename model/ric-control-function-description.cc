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

#include "ric-control-function-description.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "ControlAction-RANParameter-Item.h"
#include "RANFunctionDefinition-Control-Action-Item.h"
#include "RANFunctionDefinition-Control-Item.h"
#include "RANFunctionDefinition-Control.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RicControlFunctionDescription");

RicControlFunctionDescription::RicControlFunctionDescription()
{
    E2SM_RC_RANFunctionDefinition_t* descriptor = new E2SM_RC_RANFunctionDefinition_t();
    memset(descriptor, 0, sizeof(E2SM_RC_RANFunctionDefinition_t));
    FillAndEncodeRCFunctionDescription(descriptor);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_RC_RANFunctionDefinition, descriptor);
    delete descriptor;
}

RicControlFunctionDescription::~RicControlFunctionDescription()
{
    free(m_buffer);
    m_size = 0;
}

void
RicControlFunctionDescription::Encode(E2SM_RC_RANFunctionDefinition_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = 0; // disable stack bounds checking
    asn_encode_to_new_buffer_result_s encodedMsg =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_RC_RANFunctionDefinition,
                                 descriptor);

    if (encodedMsg.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the E2SM-RC RAN Function Definition, errno: "
                       << strerror(errno) << ", failed_type " << encodedMsg.result.failed_type->name
                       << ", structure_ptr " << encodedMsg.result.structure_ptr);
    }

    m_buffer = encodedMsg.buffer;
    m_size = encodedMsg.result.encoded;
}

static void
AddControlActionRanParameter(
    RANFunctionDefinition_Control_Action_Item::
        RANFunctionDefinition_Control_Action_Item__ran_ControlActionParameters_List* list,
    long paramId,
    const std::string& paramName)
{
    auto* paramItem =
        (ControlAction_RANParameter_Item_t*)calloc(1, sizeof(ControlAction_RANParameter_Item_t));
    paramItem->ranParameter_ID = paramId;
    OCTET_STRING_fromString(&paramItem->ranParameter_name, paramName.c_str());
    ASN_SEQUENCE_ADD(&list->list, paramItem);
}

void
RicControlFunctionDescription::FillAndEncodeRCFunctionDescription(
    E2SM_RC_RANFunctionDefinition_t* ranfunc_desc)
{
    const std::string shortName = "ORAN-E2SM-RC";
    const std::string description = "RAN Control Service Model v3.01";
    const std::string oid = "1.3.6.1.4.1.53148.1.3.2.3";

    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_ShortName, shortName.c_str());
    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_Description, description.c_str());
    OCTET_STRING_fromString(&ranfunc_desc->ranFunction_Name.ranFunction_E2SM_OID, oid.c_str());

    long* inst = (long*)calloc(1, sizeof(long));
    *inst = 0;
    ranfunc_desc->ranFunction_Name.ranFunction_Instance = inst;

    // RANFunctionDefinition-Control
    ranfunc_desc->ranFunctionDefinition_Control =
        (struct RANFunctionDefinition_Control*)calloc(1, sizeof(struct RANFunctionDefinition_Control));

    // Control Style 1: "Radio Bearer Control"
    auto* controlStyle1 =
        (RANFunctionDefinition_Control_Item_t*)calloc(1, sizeof(RANFunctionDefinition_Control_Item_t));
    controlStyle1->ric_ControlStyle_Type = 1;
    OCTET_STRING_fromString(&controlStyle1->ric_ControlStyle_Name, "Radio Bearer Control");
    controlStyle1->ric_ControlHeaderFormat_Type = 1;
    controlStyle1->ric_ControlMessageFormat_Type = 1;
    controlStyle1->ric_ControlOutcomeFormat_Type = 1;

    controlStyle1->ric_ControlAction_List =
        (RANFunctionDefinition_Control_Item::
             RANFunctionDefinition_Control_Item__ric_ControlAction_List*)
            calloc(1,
                   sizeof(RANFunctionDefinition_Control_Item::
                              RANFunctionDefinition_Control_Item__ric_ControlAction_List));

    // Control Action ID 6: "Slice-level PRB quota"
    auto* controlAction6 = (RANFunctionDefinition_Control_Action_Item_t*)calloc(
        1,
        sizeof(RANFunctionDefinition_Control_Action_Item_t));
    controlAction6->ric_ControlAction_ID = 6;
    OCTET_STRING_fromString(&controlAction6->ric_ControlAction_Name, "Slice-level PRB quota");

    controlAction6->ran_ControlActionParameters_List =
        (RANFunctionDefinition_Control_Action_Item::
             RANFunctionDefinition_Control_Action_Item__ran_ControlActionParameters_List*)
            calloc(1,
                   sizeof(RANFunctionDefinition_Control_Action_Item::
                              RANFunctionDefinition_Control_Action_Item__ran_ControlActionParameters_List));

    // RAN Parameters for Action 6 (Slice-level PRB quota)
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 1, "RRM Policy Ratio List");
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 2, "RRM Policy Ratio Group");
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 3, "RRM Policy SST");
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 4, "Max PRB Policy Ratio");
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 5, "Min PRB Policy Ratio");
    AddControlActionRanParameter(controlAction6->ran_ControlActionParameters_List, 6, "Dedicated PRB Policy Ratio");

    ASN_SEQUENCE_ADD(&controlStyle1->ric_ControlAction_List->list, controlAction6);

    ASN_SEQUENCE_ADD(&ranfunc_desc->ranFunctionDefinition_Control->ric_ControlStyle_List.list,
                     controlStyle1);

    Encode(ranfunc_desc);

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_RC_RANFunctionDefinition, ranfunc_desc));
}

} // namespace ns3
