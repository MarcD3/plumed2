/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2011-2023 The plumed team
   (see the PEOPLE file at the root of the distribution for a list of names)

   See http://www.plumed.org for more information.

   This file is part of plumed, version 2.

   plumed is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   plumed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with plumed.  If not, see <http://www.gnu.org/licenses/>.
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
#ifndef __PLUMED_core_ActionWithVector_h
#define __PLUMED_core_ActionWithVector_h

#include "ActionWithValue.h"
#include "ActionAtomistic.h"
#include "ActionWithArguments.h"
#include "tools/MultiValue.h"
#include <vector>

namespace PLMD {

class ActionWithVector:
  public ActionAtomistic,
  public ActionWithValue,
  public ActionWithArguments
{
private:
/// Is the calculation to be done in serial
  bool serial;  
/// The current number of active tasks
  unsigned nactive_tasks;
/// The buffer that we use (we keep a copy here to avoid resizing)
  std::vector<double> buffer;
  /// Action that must be done before this one
  ActionWithVector* action_to_do_before;
/// Actions that must be done after this one
  ActionWithVector* action_to_do_after;
/// Check if there are any grids in the stream 
  bool checkForGrids() const ;
/// Find out how many tasks we need to perform in this loop
  void getNumberOfTasks( unsigned& ntasks );
///  Run the task
  void runTask( const unsigned& taskno, MultiValue& myvals ) const ;
/// Get the size of the buffer array that holds the data we are gathering over the MPI loop
  void getSizeOfBuffer( const unsigned& nactive_tasks, unsigned& bufsize );
/// Get the number of derivatives in the stream
  void getNumberOfStreamedDerivatives( unsigned& nderivatives );
/// Add this action to the recursive chain
  bool addActionToChain( const std::vector<std::string>& alabels, ActionWithVector* act );
/// Get every the label of every value that is calculated in this chain
  void getAllActionLabelsInChain( std::vector<std::string>& mylabels ) const ;
/// Get the distinct arguments to use for applying derivatives
  ActionWithVector* getFirstNonStream();
/// Get all the actions we need for this action
  void getAllActionsRequired( std::vector<const ActionWithValue*>& allvals ) const;
protected:
/// Assert if this action is part of a chain
  bool done_in_chain;
/// The locations of the starts for the argument derivatives
  std::vector<unsigned> arg_deriv_starts;
/// The arguments in the input that are distinct
  std::vector<ActionWithValue*> distinct_arguments;
/// Are we running this command in a chain
  bool actionInChain() const ;
/// This sets up the arguments at the start of the calculation
  unsigned buildArgumentStore( const unsigned& argstart );
/// Get the position of the argument in the streamm and set it if we need to
  unsigned getArgumentPositionInStream( const unsigned& jder, MultiValue& myvals ) const ;
/// Run all the tasks in the list
  void runAllTasks();  
public:
  static void registerKeywords( Keywords& keys );
  explicit ActionWithVector(const ActionOptions&);
  void lockRequests() override;
  void unlockRequests() override;
  void calculateNumericalDerivatives(ActionWithValue* av) override { plumed_merror("cannot calculate numerical derivative for this type of action"); }
/// We override clearDerivatives here to prevent data in streams from being deleted
  void clearDerivatives( const bool& force=false ) override;
/// Get the action that does the calculation
  ActionWithVector* getActionThatCalculates();
/// Check if we can be after another ActionWithVector
  virtual bool canBeAfterInChain( ActionWithVector* av ) { return true; }
/// Get the number of quantities in the stream
  virtual void getNumberOfStreamedQuantities( unsigned& nquants, unsigned& ncols, unsigned& nmat );
/// This we override to perform each individual task
  virtual void performTask( const unsigned& current, MultiValue& myvals ) const = 0;
/// Gather the values that we intend to store in the buffer
  void gatherAccumulators( const unsigned& taskCode, const MultiValue& myvals, std::vector<double>& buffer ) const ;
/// This is to transfer data from the buffer to the final value
  void finishComputations( const std::vector<double>& buf );
/// This tells us which arguments must be treated as distinct in functions
  virtual bool mustBeTreatedAsDistinctArguments( const unsigned& argstart );
/// Do any transformations we need to do on the final value after the data has all been gathered
  virtual void transformFinalValueAndDerivatives( const std::vector<double>& buf  ) {}
};

inline
bool ActionWithVector::actionInChain() const {
  return (action_to_do_before!=NULL);
}

}

#endif
