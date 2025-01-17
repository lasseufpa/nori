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

#include "indication-message-helper.h"

namespace ns3
{

class LteIndicationMessageHelper : public IndicationMessageHelper
{
  public:
    LteIndicationMessageHelper(IndicationMessageType type, bool isOffline, bool reducedPmValues);

    ~LteIndicationMessageHelper();

    void FillCuUpValues(std::string plmId, long pdcpBytesUl, long pdcpBytesDl);

    void AddCuUpUePmItem(std::string ueImsiComplete,
                         long txBytes,
                         long txDlPackets,
                         double pdcpThroughput,
                         double pdcpLatency);

    void AddCuUpCellPmItem(double cellAverageLatency);

    void FillCuCpValues(uint16_t numActiveUes);

    void AddCuCpUePmItem(std::string ueImsiComplete, long numDrb, long drbRelAct);

  private:
};

} // namespace ns3
