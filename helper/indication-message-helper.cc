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

#include "indication-message-helper.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IndicationMessageHelper");

IndicationMessageHelper::IndicationMessageHelper(IndicationMessageType type,
                                                 bool isOffline,
                                                 bool reducePmValues)
    : m_type(type),
      m_offline(isOffline),
      m_reducePmValues(reducePmValues)
{
    switch (type){
        case IndicationMessageType::NodeLevel:
            m_msgValues.m_format = KpmIndicationMessage::MessageFormat::FORMAT1;
            break;
        case IndicationMessageType::UeLevel:
            m_msgValues.m_format = KpmIndicationMessage::MessageFormat::FORMAT3;
            break;
        default:
            NS_LOG_ERROR("Invalid indication message type");
            break;

    }
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
