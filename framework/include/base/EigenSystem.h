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

#ifndef EIGENSYSTEM_H
#define EIGENSYSTEM_H

#include "NonlinearSystem.h"

class FEProblem;

class EigenSystem : public NonlinearSystem
{
public:
  EigenSystem(FEProblem & problem, const std::string & name);
  virtual ~EigenSystem();

  /**
   * Adds a kernel
   * @param kernel_name The type of the kernel
   * @param name The name of the kernel
   * @param parameters Kernel parameters
   */
  virtual void addKernel(const std::string & kernel_name, const std::string & name, InputParameters parameters);

  /**
   * System or kernel tags
   */
  enum SYSTEMTAG
  {
    ALL,
    EIGEN
  };

  /**
   * Scale the solution vector
   *
   * @param tag System tag.
   * @param factor The scaling factor.
   */
  void scaleSystemSolution(SYSTEMTAG tag, Real scaling_factor);

  /**
   * Linear combination of the solution vectors
   *
   * @param tag System tag.
   * @param fcoef Coefficients for current, old and older solutions.
   */
  void combineSystemSolution(SYSTEMTAG tag, const std::vector<Real> & coefficients);

  /**
   * Initialize the solution vector with a constant value
   *
   * @param tag System tag.
   * @param v The value.
   */
  void initSystemSolution(SYSTEMTAG tag, Real v);
  void initSystemSolutionOld(SYSTEMTAG tag, Real v);

  /**
   * Ask eigenkernels to operate on old or current solution vectors
   */
  void eigenKernelOnOld();
  void eigenKernelOnCurrent();

  /**
   * Build DoF indices for a system
   */
  void buildSystemDoFIndices(SYSTEMTAG tag = ALL);

  /**
   * Return if eigen kernels should be on old solution
   */
  bool activeOnOld();

  /**
   * Get variable names of the eigen system
   */
  const std::set<VariableName> & getEigenVariableNames() const { return _eigen_var_names; }

protected:

  std::set<VariableName> _eigen_var_names;
  bool _all_eigen_vars;
  std::set<dof_id_type> _eigen_var_indices;

  bool _active_on_old;
};

#endif /* EIGENSYSTEM_H */
