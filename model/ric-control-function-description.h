/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 */

#ifndef RIC_CONTROL_FUNCTION_DESCRIPTION_H
#define RIC_CONTROL_FUNCTION_DESCRIPTION_H

#include "function-description.h"

#include "ns3/object.h"

extern "C"
{
#include "E2SM-RC-RANFunctionDefinition.h"
}

namespace ns3
{

class RicControlFunctionDescription : public FunctionDescription
{
  public:
    RicControlFunctionDescription();
    ~RicControlFunctionDescription();

  private:
    void FillAndEncodeRCFunctionDescription(E2SM_RC_RANFunctionDefinition_t* descriptor);
    void Encode(E2SM_RC_RANFunctionDefinition_t* descriptor);
};
} // namespace ns3

#endif /* RIC_CONTROL_FUNCTION_DESCRIPTION_H */
