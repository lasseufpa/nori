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

#include "ns3/object.h"
#include "kpm-metrics-defs.h" 

extern "C"
{
#include "E2SM-KPM-IndicationHeader.h"
#include "E2SM-KPM-IndicationMessage.h"
#include "E2SM-KPM-RANfunction-Description.h"
#include "asn1c-types.h"
#include "RIC-EventTriggerStyle-Item.h"
#include "RIC-ReportStyle-Item.h"
#include "MeasurementInfo-Action-List.h"
#include "MeasurementInfo-Action-Item.h"
}

namespace ns3
{

class KpmFunctionDescription : public SimpleRefCount<KpmFunctionDescription>
{
  public:
    KpmFunctionDescription();
    ~KpmFunctionDescription();

    // Get the encoded KPM function description, ready to be sent in E2AP messages.
    void* m_buffer;
    size_t m_size;

  private:
    void FillAndEncodeKpmFunctionDescription(E2SM_KPM_RANfunction_Description_t* descriptor);
    void Encode(E2SM_KPM_RANfunction_Description_t* descriptor);
};

} // namespace ns3