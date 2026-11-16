#pragma once

#include "function-description.h"

#include <string>

namespace ns3
{

/**
 * @ingroup nori-ccc
 * @brief E2SM-CCC RAN function definition (JSON).
 */
class CccFunctionDescription : public FunctionDescription
{
  public:
    CccFunctionDescription();
    ~CccFunctionDescription();

  private:
    void BuildFunctionDescription();
};

} // namespace ns3
