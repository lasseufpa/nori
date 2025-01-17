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

#include "function-description.h"

#include "ns3/object.h"

extern "C"
{
#include "E2SM-RC-RANFunctionDefinition.h"
}

namespace ns3
{

class RicControlFunctionDescription : public FunctionDescription
{
  public:
    RicControlFunctionDescription();
    ~RicControlFunctionDescription();

  private:
    void FillAndEncodeRCFunctionDescription(E2SM_RC_RANFunctionDefinition_t* descriptor);
    void Encode(E2SM_RC_RANFunctionDefinition_t* descriptor);
};
} // namespace ns3

#pragma once
