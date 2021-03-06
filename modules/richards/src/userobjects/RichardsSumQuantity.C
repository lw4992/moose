/*****************************************/
/* Written by andrew.wilkins@csiro.au    */
/* Please contact me if you make changes */
/*****************************************/

#include "RichardsSumQuantity.h"

template<>
InputParameters validParams<RichardsSumQuantity>()
{
  InputParameters params = validParams<GeneralUserObject>();

  return params;
}

RichardsSumQuantity::RichardsSumQuantity(const std::string & name, InputParameters parameters) :
    GeneralUserObject(name, parameters),
    _total_outflow_mass(0)
{
}

RichardsSumQuantity::~RichardsSumQuantity()
{
}

void
RichardsSumQuantity::zero()
{
  _total_outflow_mass = 0;
}

void
RichardsSumQuantity::add(Real contrib)
{
  _total_outflow_mass += contrib;
}

void
RichardsSumQuantity::initialize()
{
}

void
RichardsSumQuantity::execute()
{
}

void
RichardsSumQuantity::finalize()
{
  gatherSum(_total_outflow_mass);
}

Real
RichardsSumQuantity::getValue() const
{
  return _total_outflow_mass;
}
