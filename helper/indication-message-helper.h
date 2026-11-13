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
#include "ns3/simple-ref-count.h"

namespace ns3
{

// Short-lived message builder created with Create<>(): reference counting only
// (ns-3.48 forbids Create<>() for Object subclasses).
class IndicationMessageHelper : public SimpleRefCount<IndicationMessageHelper>
{
  public:

    enum class IndicationMessageType
    {
      NodeLevel = 0,
      UeLevel = 1,
    };

    IndicationMessageHelper(IndicationMessageType type, bool isOffline, bool reducePmValues);
    virtual ~IndicationMessageHelper(); // SimpleRefCount deletes through the base type

    Ptr<KpmIndicationMessage> CreateIndicationMessage();

    const bool& IsOffline() const
    {
      return m_offline;
    }

  protected:
    IndicationMessageType m_type;
    bool m_offline;
    bool m_reducePmValues;
    KpmIndicationMessage::KpmIndicationMessageValues m_msgValues;

};

} // namespace ns3
