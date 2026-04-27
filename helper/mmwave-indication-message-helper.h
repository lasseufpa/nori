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
#include "ns3/kpm-metrics-defs.h"

namespace ns3
{

class MmWaveIndicationMessageHelper : public IndicationMessageHelper
{
  public:
    MmWaveIndicationMessageHelper(bool isOffline, bool reducedPmValues);
 
    ~MmWaveIndicationMessageHelper();

void AddCuUpUePmItem(std::string ueImsiComplete, 
                         std::string plmId, 
                         long txPdcpPduBytesNrRlc,
                         long txPdcpPduNrRlc, 
                         double pdcpThroughput);

    // Plano de Controle (Antigo CU-CP)
    void AddCuCpUePmItem(std::string ueImsiComplete,
                         long numDrb,
                         long drbRelAct);

    // Unidade Distribuída - Itens por UE (Antigo DU UE)
    void AddDuUePmItem(std::string ueImsiComplete,
                       uint64_t nrCellId,
                       long macPduUe,
                       long macPduInitialUe,
                       long macQpsk,
                       long mac16Qam,
                       long mac64Qam,
                       long macRetx,
                       long macPrb,
                       long macMac04, long macMac59, long macMac1014, // MCS Bins
                       long macMac1519, long macMac2024, long macMac2529,
                       long macSinrBin1, long macSinrBin2, long macSinrBin3, // SINR Bins
                       long macSinrBin4, long macSinrBin5, long macSinrBin6,
                       long macSinrBin7,
                       long rlcBufferOccup,
                       double drbThrDlUeid);

    // Unidade Distribuída - Itens por Célula (Antigo DU Cell)
    void AddDuCellPmItem(uint64_t nrCellId, 
                         double prbUtilizationDl,
                         long activeUeDl);

    // void AddDuCellResRepPmItem(uint64_t nrCellId, Ptr<CellResourceReport> cellResRep);

  private:
};

} // namespace ns3
