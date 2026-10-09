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

#pragma once

#include "function-description.h"

#include "ns3/object.h"

extern "C"
{
#include "E2SM-RC-RANFunctionDefinition.h"
#include "RANFunctionDefinition-Control.h"
#include "RANFunctionDefinition-Control-Item.h"
#include "RANFunctionDefinition-Control-Action-Item.h"
#include "ControlAction-RANParameter-Item.h"
}

namespace ns3
{

class RicControlFunctionDescription : public FunctionDescription
{
  public:
    RicControlFunctionDescription();
    ~RicControlFunctionDescription();

  private:
    /**
     * Encodes the RAN Function Definition item for the E2SM-RC Service Model v3.01.
     *
     * @param descriptor the RAN Function Definition item
     */
    void FillAndEncodeRCFunctionDescription(E2SM_RC_RANFunctionDefinition_t* descriptor);
    void Encode(E2SM_RC_RANFunctionDefinition_t* descriptor);
};

} // namespace ns3
