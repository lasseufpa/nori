#pragma once

#include "function-description.h"

#include <string>

namespace ns3
{

class CccFunctionDescription : public FunctionDescription
{
  public:
    CccFunctionDescription();
    ~CccFunctionDescription();

  private:
    void BuildFunctionDescription();
};

} // namespace ns3
