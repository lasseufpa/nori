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

#include <string>

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
    /// Service model OID sent in the RANfunction-Item of the E2 Setup Request
    /// (e.g. "1.3.6.1.4.1.53148.1.3.2.2" for E2SM-KPM v3.00).
    std::string m_oid;

    // TODO improve the abstraction
    //   private:
    //     virtual void Encode (E2SM_KPM_RANfunction_Description_t* descriptor);
};

} // namespace ns3
