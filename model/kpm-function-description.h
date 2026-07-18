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

#pragma once

#include "function-description.h"

#include "ns3/object.h"

extern "C"
{
#include "ESM-KPM-RANfunction-Description.h"
#include "MeasurementInfo-Action-Item.h"
#include "MeasurementInfo-Action-List.h"
#include "asn1c-types.h"
}

namespace ns3
{

class KpmFunctionDescription : public FunctionDescription
{
  public:
    KpmFunctionDescription();
    ~KpmFunctionDescription();

  private:
    /**
     * Encodes the RAN Function Description item for the KPM Service Model.
     *
     * @param kpmFunctionDescription the RAN Function Description item
     */
    void FillAndEncodeKpmFunctionDescription(E2SM_KPM_RANfunction_Description_t* descriptor);
    void Encode(E2SM_KPM_RANfunction_Description_t* descriptor);
};

} // namespace ns3
