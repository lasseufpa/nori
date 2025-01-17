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

IndicationMessageHelper::IndicationMessageHelper(IndicationMessageType type,
                                                 bool isOffline,
                                                 bool reducedPmValues)
    : m_type(type),
      m_offline(isOffline),
      m_reducedPmValues(reducedPmValues)
{
    if (!m_offline)
    {
        switch (type)
        {
        case IndicationMessageType::CuUp:
            m_cuUpValues = Create<OCuUpContainerValues>();
            break;

        case IndicationMessageType::CuCp:
            m_cuCpValues = Create<OCuCpContainerValues>();
            m_msgValues.m_cellObjectId = "NRCellCU";
            break;

        case IndicationMessageType::Du:
            m_duValues = Create<ODuContainerValues>();
            break;

        default:

            break;
        }
    }
}

void
IndicationMessageHelper::FillBaseCuUpValues(std::string plmId)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::CuUp, "Wrong function for this object");
    m_cuUpValues->m_plmId = plmId;
    m_msgValues.m_pmContainerValues = m_cuUpValues;
}

void
IndicationMessageHelper::FillBaseCuCpValues(uint16_t numActiveUes)
{
    NS_ABORT_MSG_IF(m_type != IndicationMessageType::CuCp, "Wrong function for this object");
    m_cuCpValues->m_numActiveUes = numActiveUes;
    m_msgValues.m_pmContainerValues = m_cuCpValues;
}

IndicationMessageHelper::~IndicationMessageHelper()
{
}

Ptr<KpmIndicationMessage>
IndicationMessageHelper::CreateIndicationMessage()
{
    return Create<KpmIndicationMessage>(m_msgValues);
}

} // namespace ns3
