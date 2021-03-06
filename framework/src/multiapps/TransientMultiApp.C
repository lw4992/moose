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
#include "TransientMultiApp.h"

#include "TimeStepper.h"
#include "LayeredSideFluxAverage.h"
#include "AllLocalDofIndicesThread.h"

// libMesh
#include "libmesh/mesh_tools.h"

template<>
InputParameters validParams<TransientMultiApp>()
{
  InputParameters params = validParams<MultiApp>();
  params += validParams<TransientInterface>();

  params.addParam<bool>("sub_cycling", false, "Set to true to allow this MultiApp to take smaller timesteps than the rest of the simulation.  More than one timestep will be performed for each 'master' timestep");

  params.addParam<bool>("interpolate_transfers", false, "Only valid when sub_cycling.  This allows transferred values to be interpolated over the time frame the MultiApp is executing over when sub_cycling");

  params.addParam<bool>("detect_steady_state", false, "If true then while sub_cycling a steady state check will be done.  In this mode output will only be done once the MultiApp reaches the target time or steady state is reached");

  params.addParam<Real>("steady_state_tol", 1e-8, "The relative difference between the new solution and the old solution that will be considered to be at steady state");

  params.addParam<bool>("output_sub_cycles", false, "If true when sub_cycling every sub-cycle will be output.");

  params.addParam<unsigned int>("max_failures", 0, "Maximum number of solve failures tolerated while sub_cycling.");

  params.addParam<bool>("tolerate_failure", false, "If true this MultiApp won't participate in dt decisions and will always be fast-forwarded to the current time.");

  params.addParam<bool>("catch_up", false, "If true this will allow failed solves to attempt to 'catch up' using smaller timesteps.");

  params.addParam<Real>("max_catch_up_steps", 2, "Maximum number of steps to allow an app to take when trying to catch back up after a failed solve.");

  return params;
}


TransientMultiApp::TransientMultiApp(const std::string & name, InputParameters parameters):
    MultiApp(name, parameters),
    _sub_cycling(getParam<bool>("sub_cycling")),
    _interpolate_transfers(getParam<bool>("interpolate_transfers")),
    _detect_steady_state(getParam<bool>("detect_steady_state")),
    _steady_state_tol(getParam<Real>("steady_state_tol")),
    _output_sub_cycles(getParam<bool>("output_sub_cycles")),
    _max_failures(getParam<unsigned int>("max_failures")),
    _tolerate_failure(getParam<bool>("tolerate_failure")),
    _failures(0),
    _catch_up(getParam<bool>("catch_up")),
    _max_catch_up_steps(getParam<Real>("max_catch_up_steps")),
    _first(declareRestartableData<bool>("first", true)),
    _auto_advance(false)
{
  // Transfer interpolation only makes sense for sub-cycling solves
  if (_interpolate_transfers && !_sub_cycling)
    mooseError("MultiApp " << _name << " is set to interpolate_transfers but is not sub_cycling!  That is not valid!");
}

TransientMultiApp::~TransientMultiApp()
{
  if (!_has_an_app)
    return;

  MPI_Comm swapped = Moose::swapLibMeshComm(_my_comm);

  for(unsigned int i=0; i<_my_num_apps; i++)
  {
    Transient * ex = _transient_executioners[i];

    ex->postExecute();
  }

  // Swap back
  Moose::swapLibMeshComm(swapped);
}

NumericVector<Number> &
TransientMultiApp::appTransferVector(unsigned int app, std::string var_name)
{
  if (std::find(_transferred_vars.begin(), _transferred_vars.end(), var_name) == _transferred_vars.end())
    _transferred_vars.push_back(var_name);

  if (_interpolate_transfers)
    return appProblem(app)->getAuxiliarySystem().system().get_vector("transfer");

  return appProblem(app)->getAuxiliarySystem().solution();
}

void
TransientMultiApp::init()
{
  MultiApp::init();

  if (!_has_an_app)
    return;

  MPI_Comm swapped = Moose::swapLibMeshComm(_my_comm);

  if (_has_an_app)
  {
    _transient_executioners.resize(_my_num_apps);
    // Grab Transient Executioners from each app
    for(unsigned int i=0; i<_my_num_apps; i++)
      setupApp(i);
  }

  // Swap back
  Moose::swapLibMeshComm(swapped);
}

void
TransientMultiApp::solveStep(Real dt, Real target_time, bool auto_advance)
{
  if (_sub_cycling && !auto_advance)
    mooseError("TransientMultiApp with sub_cycling=true is not compatible with auto_advance=false");

  if (_catch_up && !auto_advance)
    mooseError("TransientMultiApp with catch_up=true is not compatible with auto_advance=false");

  if (!_has_an_app)
    return;

  _auto_advance = auto_advance;

  Moose::out << "Solving MultiApp " << _name << std::endl;

  // "target_time" must always be in global time
  target_time += _app.getGlobalTimeOffset();

  MPI_Comm swapped = Moose::swapLibMeshComm(_my_comm);

  int rank;
  int ierr;
  ierr = MPI_Comm_rank(_orig_comm, &rank); mooseCheckMPIErr(ierr);

  for(unsigned int i=0; i<_my_num_apps; i++)
  {

    FEProblem * problem = appProblem(_first_local_app + i);
    OutputWarehouse & output_warehouse = _apps[i]->getOutputWarehouse();
    output_warehouse.timestepSetup();

    Transient * ex = _transient_executioners[i];

    // The App might have a different local time from the rest of the problem
    Real app_time_offset = _apps[i]->getGlobalTimeOffset();

    if ((ex->getTime() + app_time_offset) + 2e-14 >= target_time) // Maybe this MultiApp was already solved
      continue;

    if (_sub_cycling)
    {
      Real time_old = ex->getTime() + app_time_offset;

      if (_interpolate_transfers)
      {
        AuxiliarySystem & aux_system = problem->getAuxiliarySystem();
        System & libmesh_aux_system = aux_system.system();

        NumericVector<Number> & solution = *libmesh_aux_system.solution;
        NumericVector<Number> & transfer_old = libmesh_aux_system.get_vector("transfer_old");

        solution.close();

        // Save off the current auxiliary solution
        transfer_old = solution;

        transfer_old.close();

        // Snag all of the local dof indices for all of these variables
        AllLocalDofIndicesThread aldit(libmesh_aux_system, _transferred_vars);
        ConstElemRange & elem_range = *problem->mesh().getActiveLocalElementRange();
        Threads::parallel_reduce(elem_range, aldit);

        _transferred_dofs = aldit._all_dof_indices;
      }

      /// \todo{remove ex->allowOutput()}
      if (_output_sub_cycles)
      {
        ex->allowOutput(true);
        output_warehouse.allowOutput(true);
      }
      else
      {
        ex->allowOutput(false);
        output_warehouse.allowOutput(false);
      }

      ex->setTargetTime(target_time-app_time_offset);

//      unsigned int failures = 0;

      bool at_steady = false;

      // Now do all of the solves we need
      while(true)
      {
        if (_first != true)
          ex->incrementStepOrReject();
        _first = false;

        if (!(!at_steady && ex->getTime() + app_time_offset + 2e-14 < target_time))
          break;

        ex->computeDT();

        if (_interpolate_transfers)
        {
          // See what time this executioner is going to go to.
          Real future_time = ex->getTime() + app_time_offset + ex->getDT();

          // How far along we are towards the target time:
          Real step_percent = (future_time - time_old) / (target_time - time_old);

          Real one_minus_step_percent = 1.0 - step_percent;

          // Do the interpolation for each variable that was transferred to
          FEProblem * problem = appProblem(_first_local_app + i);
          AuxiliarySystem & aux_system = problem->getAuxiliarySystem();
          System & libmesh_aux_system = aux_system.system();

          NumericVector<Number> & solution = *libmesh_aux_system.solution;
          NumericVector<Number> & transfer = libmesh_aux_system.get_vector("transfer");
          NumericVector<Number> & transfer_old = libmesh_aux_system.get_vector("transfer_old");

          solution.close(); // Just to be sure
          transfer.close();
          transfer_old.close();

          std::set<dof_id_type>::iterator it  = _transferred_dofs.begin();
          std::set<dof_id_type>::iterator end = _transferred_dofs.end();

          for(; it != end; ++it)
          {
            dof_id_type dof = *it;
            solution.set(dof, (transfer_old(dof) * one_minus_step_percent) + (transfer(dof) * step_percent));
//            solution.set(dof, transfer_old(dof));
//            solution.set(dof, transfer(dof));
//            solution.set(dof, 1);
          }

          solution.close();
        }

        ex->takeStep();

        bool converged = ex->lastSolveConverged();

        if (!converged)
        {
          mooseWarning("While sub_cycling "<<_name<<_first_local_app+i<<" failed to converge!"<<std::endl);
          _failures++;

          if (_failures > _max_failures)
            mooseError("While sub_cycling "<<_name<<_first_local_app+i<<" REALLY failed!"<<std::endl);
        }

        Real solution_change_norm = ex->getSolutionChangeNorm();

        if (_detect_steady_state)
          Moose::out << "Solution change norm: " << solution_change_norm << std::endl;

        if (converged && _detect_steady_state && solution_change_norm < _steady_state_tol)
        {
          Moose::out << "Detected Steady State!  Fast-forwarding to " << target_time << std::endl;

          at_steady = true;

          // Force it to output right now \todo{Remove}
          ex->forceOutput();

          // Indicate that the next output call (occurs in ex->endStep()) should output, regarless of intervals etc...
          output_warehouse.forceOutput();

          // Clean up the end
          ex->endStep(target_time-app_time_offset);
        }
        else
          ex->endStep();
      }

      // If we were looking for a steady state, but didn't reach one, we still need to output one more time
      if (!at_steady)
      {
        output_warehouse.forceOutput();
        output_warehouse.outputStep();
        ex->forceOutput(); // \todo{Remove}
      }

    }
    else if (_tolerate_failure)
    {
      ex->takeStep(dt);
      ex->forceOutput(); // \todo{Remove}
      output_warehouse.forceOutput();
      ex->endStep(target_time-app_time_offset);
    }
    else
    {
      Moose::out << "Solving Normal Step!" << std::endl;
      if (auto_advance)
        if (_first != true)
          ex->incrementStepOrReject();

      if (auto_advance)
        output_warehouse.allowOutput(true);

      ex->takeStep(dt);

      if (auto_advance)
      {
        ex->endStep();

        if (!ex->lastSolveConverged())
        {
          mooseWarning(_name << _first_local_app+i << " failed to converge!" << std::endl);

          if (_catch_up)
          {
            Moose::out << "Starting Catch Up!" << std::endl;

            bool caught_up = false;

            unsigned int catch_up_step = 0;

            Real catch_up_dt = dt/2;

            ex->allowOutput(false); // Don't output while catching up \todo{Remove}
            //  output_warehouse.allowOutput(false);

            while(!caught_up && catch_up_step < _max_catch_up_steps)
            {
              Moose::err << "Solving " << _name << "catch up step " << catch_up_step << std::endl;
              ex->incrementStepOrReject();

              ex->computeDT();
              ex->takeStep(catch_up_dt); // Cut the timestep in half to try two half-step solves

              if (ex->lastSolveConverged())
              {
                if (ex->getTime() + app_time_offset + ex->timestepTol()*std::abs(ex->getTime()) >= target_time)
                {
                  ex->forceOutput(); // This is here so that it is called before endStep() // \todo{Remove}
                  output_warehouse.forceOutput();
                  output_warehouse.outputStep();
                  caught_up = true;
                }
              }
              else
                catch_up_dt /= 2.0;

              //output_warehouse.forceOutput();
              ex->endStep(); // This is here so it is called after forceOutput()

              catch_up_step++;
            }

            if (!caught_up)
              mooseError(_name << " Failed to catch up!\n");

            output_warehouse.allowOutput(true);
            ex->allowOutput(true); // \todo{Remove}
          }
        }
      }
    }
  }

  _first = false;

  // Swap back
  Moose::swapLibMeshComm(swapped);

  _transferred_vars.clear();

  Moose::out << "Finished Solving MultiApp " << _name << std::endl;
}

void
TransientMultiApp::advanceStep()
{
  if (!_auto_advance)
  {
    for(unsigned int i=0; i<_my_num_apps; i++)
    {
//      FEProblem * problem = appProblem(_first_local_app + i);
      OutputWarehouse & output_warehouse = _apps[i]->getOutputWarehouse();
      output_warehouse.timestepSetup();

      Transient * ex = _transient_executioners[i];

      output_warehouse.allowOutput(true);
      ex->endStep();
      ex->incrementStepOrReject();
    }
  }
}

Real
TransientMultiApp::computeDT()
{
  if (_sub_cycling) // Bow out of the timestep selection dance
    return std::numeric_limits<Real>::max();

  Real smallest_dt = std::numeric_limits<Real>::max();

  if (_has_an_app)
  {
    MPI_Comm swapped = Moose::swapLibMeshComm(_my_comm);

    for(unsigned int i=0; i<_my_num_apps; i++)
    {
      Transient * ex = _transient_executioners[i];
      ex->computeDT();
      Real dt = ex->getDT();

      smallest_dt = std::min(dt, smallest_dt);
    }

    // Swap back
    Moose::swapLibMeshComm(swapped);
  }

  if (_tolerate_failure) // Bow out of the timestep selection dance, we do this down here because we need to call computeConstrainedDT at least once for these executioners...
    return std::numeric_limits<Real>::max();


  Parallel::min(smallest_dt);
  return smallest_dt;
}

void
TransientMultiApp::resetApp(unsigned int global_app, Real /*time*/)  // FIXME: Note that we are passing in time but also grabbing it below
{
  if (hasLocalApp(global_app))
  {
    unsigned int local_app = globalAppToLocal(global_app);

    // Grab the current time the App is at so we can start the new one at the same place
    Real time = _transient_executioners[local_app]->getTime() + _apps[local_app]->getGlobalTimeOffset();

    // Extract the file numbers from the output, so that the numbering is maintained after reset
    std::map<std::string, unsigned int> m = _apps[local_app]->getOutputWarehouse().getFileNumbers();

    // Reset the Multiapp
    MultiApp::resetApp(global_app, time);

    // Reset the file numbers of the newly reset apps
    _apps[local_app]->getOutputWarehouse().setFileNumbers(m);

    MPI_Comm swapped = Moose::swapLibMeshComm(_my_comm);

    setupApp(local_app, time, false);

    // Swap back
    Moose::swapLibMeshComm(swapped);
  }
}

void
TransientMultiApp::setupApp(unsigned int i, Real /*time*/, bool output_initial)  // FIXME: Should we be passing time?
{

  MooseApp * app = _apps[i];
  Transient * ex = dynamic_cast<Transient *>(app->getExecutioner());
  if (!ex)
    mooseError("MultiApp " << _name << " is not using a Transient Executioner!");

  // Get the FEProblem and OutputWarehouse for the current MultiApp
  FEProblem * problem = appProblem(_first_local_app + i);
  OutputWarehouse & output_warehouse = _apps[i]->getOutputWarehouse();

  if (!output_initial)
  {
    ex->outputInitial(false);//\todo{Remove; handled within ex->init()}
    output_warehouse.allowOutput(false);
  }

  // Set the file numbers of the i-th app to that of the parent app
  output_warehouse.setFileNumbers(app->getOutputFileNumbers());

  // Call initialization method of Executioner (Note, this preforms the output of the initial time step, if desired)
  ex->init();

  // Enable output after setup
  output_warehouse.allowOutput(true);

  if (_interpolate_transfers)
  {
    AuxiliarySystem & aux_system = problem->getAuxiliarySystem();
    System & libmesh_aux_system = aux_system.system();

    // We'll store a copy of the auxiliary system's solution at the old time in here
    libmesh_aux_system.add_vector("transfer_old", false);

    // This will be where we'll transfer the value to for the "target" time
    libmesh_aux_system.add_vector("transfer", false);
  }

  ex->preExecute();
  problem->copyOldSolutions();
  _transient_executioners[i] = ex;

  if (_detect_steady_state || _tolerate_failure)
  {
    _apps[i]->getOutputWarehouse().allowOutput(false);
    ex->allowOutput(false);
  }
}
