/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * NOTE: This file is a legacy LTE helper that was based on the KPM v2
 * container architecture (O-DU/O-CU-UP/O-CU-CP). In the KPM v3 migration,
 * the container-based approach was replaced with unified MeasurementData.
 * All methods are currently stubbed with warnings until this helper is
 * refactored for the new KPM v3 architecture or removed.
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 */

#include "lte-indication-message-helper.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LteIndicationMessageHelper");

LteIndicationMessageHelper::LteIndicationMessageHelper(IndicationMessageType type,
                                                       bool isOffline,
                                                       bool reducedPmValues)
    : IndicationMessageHelper(type, isOffline, reducedPmValues)
{
    NS_LOG_WARN("LteIndicationMessageHelper: Legacy v2 helper — methods are stubbed for KPM v3 migration.");
}

LteIndicationMessageHelper::~LteIndicationMessageHelper()
{
}

void
LteIndicationMessageHelper::AddCuUpUePmItem(std::string ueImsiComplete,
                                            long txBytes,
                                            long txDlPackets,
                                            double pdcpThroughput,
                                            double pdcpLatency)
{
    // TODO: Refactor for KPM v3 MeasurementData architecture
    NS_LOG_WARN("LteIndicationMessageHelper::AddCuUpUePmItem is not yet implemented for KPM v3.");
}

void
LteIndicationMessageHelper::AddCuUpCellPmItem(double cellAverageLatency)
{
    // TODO: Refactor for KPM v3 MeasurementData architecture
    NS_LOG_WARN("LteIndicationMessageHelper::AddCuUpCellPmItem is not yet implemented for KPM v3.");
}

void
LteIndicationMessageHelper::FillCuUpValues(std::string plmId, long pdcpBytesUl, long pdcpBytesDl)
{
    // TODO: Refactor for KPM v3 MeasurementData architecture
    NS_LOG_WARN("LteIndicationMessageHelper::FillCuUpValues is not yet implemented for KPM v3.");
}

void
LteIndicationMessageHelper::FillCuCpValues(uint16_t numActiveUes)
{
    // TODO: Refactor for KPM v3 MeasurementData architecture
    NS_LOG_WARN("LteIndicationMessageHelper::FillCuCpValues is not yet implemented for KPM v3.");
}

void
LteIndicationMessageHelper::AddCuCpUePmItem(std::string ueImsiComplete, long numDrb, long drbRelAct)
{
    // TODO: Refactor for KPM v3 MeasurementData architecture
    NS_LOG_WARN("LteIndicationMessageHelper::AddCuCpUePmItem is not yet implemented for KPM v3.");
}

} // namespace ns3
