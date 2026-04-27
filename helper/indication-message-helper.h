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

#include "ns3/kpm-indication.h"

namespace ns3
{

class IndicationMessageHelper : public Object
{
  public:
    IndicationMessageHelper(bool isOffline, bool reducedPmValues);

    ~IndicationMessageHelper();

    Ptr<KpmIndicationMessage> CreateIndicationMessage();
    void AddMetricValue(std::string metricName, double value, KpmMeasurementLabelValues labels = {});


    const bool& IsOffline() const
    {
        return m_offline;
    }

  protected:

    bool m_offline;
    bool m_reducedPmValues;
    KpmIndicationMessage::KpmIndicationMessageValues m_msgValues;

};

} // namespace ns3
