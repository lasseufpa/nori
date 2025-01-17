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

// extern "C" {
//   #include "E2SM-KPM-RANfunction-Description.h"
//   #include "E2SM-KPM-IndicationHeader.h"
//   #include "E2SM-KPM-IndicationMessage.h"
//   #include "RAN-Container.h"
//   #include "PF-Container.h"
//   #include "OCUUP-PF-Container.h"
//   #include "PF-ContainerListItem.h"
//   #include "asn1c-types.h"
// }

namespace ns3
{

class FunctionDescription : public SimpleRefCount<FunctionDescription>
{
  public:
    FunctionDescription();
    ~FunctionDescription();

    void* m_buffer;
    size_t m_size;

    // TODO improve the abstraction
    //   private:
    //     virtual void Encode (E2SM_KPM_RANfunction_Description_t* descriptor);
};

} // namespace ns3
