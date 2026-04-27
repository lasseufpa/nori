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

#include "indication-message-helper.h"

namespace ns3
{

IndicationMessageHelper::IndicationMessageHelper(
                                                 bool isOffline,
                                                 bool reducedPmValues)
     : m_offline(isOffline),
      m_reducedPmValues(reducedPmValues)
{

}
IndicationMessageHelper::~IndicationMessageHelper()
{
}

void 
IndicationMessageHelper::AddMetricValue(std::string metricName, double value, KpmMeasurementLabelValues labels)
{
    // 1. Criar os meta-dados da métrica (Coluna)
    KpmMeasurementInfoItemValues infoItem;
    infoItem.m_measName = metricName;
    infoItem.m_labels = labels; 
    
    // 2. Criar o registro do valor (Célula/Linha)
    KpmMeasurementRecordValues recordItem;
    recordItem.m_values.push_back(value); 

    // List
    m_msgValues.m_measInfoList.push_back(infoItem);
    m_msgValues.m_measData.push_back(recordItem);
}

Ptr<KpmIndicationMessage>
IndicationMessageHelper::CreateIndicationMessage()
{
    return Create<KpmIndicationMessage>(m_msgValues);
}

} // namespace ns3
