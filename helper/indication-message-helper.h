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

#include "ns3/kpm-indication.h"

namespace ns3
{

class IndicationMessageHelper : public Object
{
  public:
    enum class IndicationMessageType
    {
        CuCp = 0,
        CuUp = 1,
        Du = 2
    };
    IndicationMessageHelper(IndicationMessageType type, bool isOffline, bool reducedPmValues);

    ~IndicationMessageHelper();

    Ptr<KpmIndicationMessage> CreateIndicationMessage();

    const bool& IsOffline() const
    {
        return m_offline;
    }

  protected:
    void FillBaseCuUpValues(std::string plmId);

    void FillBaseCuCpValues(uint16_t numActiveUes);

    IndicationMessageType m_type;
    bool m_offline;
    bool m_reducedPmValues;
    KpmIndicationMessage::KpmIndicationMessageValues m_msgValues;
    Ptr<OCuUpContainerValues> m_cuUpValues;
    Ptr<OCuCpContainerValues> m_cuCpValues;
    Ptr<ODuContainerValues> m_duValues;
};

} // namespace ns3
