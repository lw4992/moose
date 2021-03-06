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

#include "InversePowerMethod.h"

template<>
InputParameters validParams<InversePowerMethod>()
{
  InputParameters params = validParams<EigenExecutionerBase>();
  params.addParam<unsigned int>("max_power_iterations", 300, "The maximum number of power iterations");
  params.addParam<unsigned int>("min_power_iterations", 1, "Minimum number of power iterations");
  params.addParam<Real>("eig_check_tol", 1e-6, "Eigenvalue convergence tolerance");
  params.addParam<Real>("pfactor", 1e-2, "Reduce residual norm per power iteration by this factor");
  params.addParam<bool>("Chebyshev_acceleration_on", true, "If Chebyshev acceleration is turned on");
  params.addParam<Real>("k0", 1.0, "Initial guess of the eigenvalue");
  params.addParam<bool>("output_pi_history", false, "True to output solutions durint PI");
  return params;
}

InversePowerMethod::InversePowerMethod(const std::string & name, InputParameters parameters)
    :EigenExecutionerBase(name, parameters),
     _min_iter(getParam<unsigned int>("min_power_iterations")),
     _max_iter(getParam<unsigned int>("max_power_iterations")),
     _eig_check_tol(getParam<Real>("eig_check_tol")),
     _pfactor(getParam<Real>("pfactor")),
     _cheb_on(getParam<bool>("Chebyshev_acceleration_on")),
     _output_pi(getParam<bool>("output_pi_history"))
{
  _eigenvalue = getParam<Real>("k0");
  addRealParameterReporter("eigenvalue");

  if (_max_iter<_min_iter) mooseError("max_power_iterations<min_power_iterations!");
  if (_eig_check_tol<0.0) mooseError("eig_check_tol<0!");
  if (_pfactor<0.0) mooseError("pfactor<0!");
  if (getParam<bool>("output_on_final") && _output_pi)
  {
    mooseWarning("Only final solution will be outputted, output_pi_history=true will be ignored!");
    _output_pi = false;
  }
}

void
InversePowerMethod::execute()
{
  preExecute();

  // save the initial guess and mark a new time step
  _problem.copyOldSolutions();

  preSolve();
  // we currently do not check the solution difference
  Real initial_res;
  Real t0 = INIT_END;
  inversePowerIteration(_min_iter, _max_iter, _pfactor, _cheb_on, _eig_check_tol,
                        std::numeric_limits<Real>::max(), true, _output_pi, t0,
                        _eigenvalue, initial_res);
  postSolve();

  _problem.computeUserObjects(EXEC_TIMESTEP, UserObjectWarehouse::PRE_AUX);
  _problem.onTimestepEnd();
  _problem.computeAuxiliaryKernels(EXEC_TIMESTEP);
  _problem.computeUserObjects(EXEC_TIMESTEP, UserObjectWarehouse::POST_AUX);
  if (_run_custom_uo) _problem.computeUserObjects(EXEC_CUSTOM);

  if (!getParam<bool>("output_on_final"))
  {
    _problem.timeStep() = POWERITERATION_END;
    Real t = _problem.time();
    _problem.time() = _problem.timeStep();
    _output_warehouse.outputStep();
    _problem.time() = t;
  }

  Real s = normalizeSolution(_norm_execflag!=EXEC_CUSTOM && _norm_execflag!=EXEC_TIMESTEP &&
                             _norm_execflag!=EXEC_RESIDUAL);

  Moose::out << " Solution is rescaled with factor " << s << " for normalization!" << std::endl;

  if (getParam<bool>("output_on_final") || std::fabs(s-1.0)>std::numeric_limits<Real>::epsilon())
  {
    _problem.timeStep() = FINAL;
    Real t = _problem.time();
    _problem.time() = _problem.timeStep();
    _output_warehouse.outputStep();
    _problem.time() = t;
  }

  postExecute();
}

void
InversePowerMethod::postSolve()
{
  printEigenvalue();
}
