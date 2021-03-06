/****************************************************************/
/*               DO NOT MODIFY THIS HEADER                      */
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*           (c) 2010 Battelle Energy Alliance, LLC             */
/*                   ALL RIGHTS RESERVED                        */
/*                                                              */
/*          Prepared by Battelle Energy Alliance, LLC           */
/*            Under Contract No. DE-AC07-05ID14517              */
/*            With the U. S. Department of Energy               */
/*                                                              */
/*            See COPYRIGHT for full restrictions               */
/****************************************************************/

#ifndef ELEMENTVECTORL2ERROR_H
#define ELEMENTVECTORL2ERROR_H

#include "ElementIntegralPostprocessor.h"
#include "FunctionInterface.h"

class Function;

//Forward Declarations
class ElementVectorL2Error;

template<>
InputParameters validParams<ElementVectorL2Error>();

class ElementVectorL2Error :
  public ElementIntegralPostprocessor,
  public FunctionInterface
{
public:
  ElementVectorL2Error(const std::string & name, InputParameters parameters);

  /**
   * Get the L2 Error.
   */
  virtual Real getValue();

protected:
  virtual Real computeQpIntegral();

  Function * const _funcx;
  Function * const _funcy;
  Function * const _funcz;

  const VariableValue * _u; // FE solution in x
  const VariableValue * _v; // FE solution in y
  const VariableValue * _w; // FE solution in z
};

#endif //ELEMENTVECTORL2ERROR_H
