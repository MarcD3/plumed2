/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   Copyright (c) 2017-2020 The plumed team
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
#ifndef __PLUMED_core_ActionToPutData_h
#define __PLUMED_core_ActionToPutData_h

#include "ActionWithValue.h"
#include "DataPassingObject.h"

namespace PLMD {

class ActionToPutData : 
public ActionWithValue
{
friend class Atoms;
friend class PlumedMain;
private:
/// Action has been set
  bool wasset;
/// Are we not applying forces on this values
  bool noforce;
/// Is this quantity scattered over the domains
  bool scattered;
/// Is this quantity fixed
  bool fixed;
/// Is the the first step
  bool firststep;
/// Can we set data at the current time
  bool dataCanBeSet;
/// Have the original forces been scaled
  bool wasscaled;
/// The unit of the value that has been passed to plumed
  enum{n,e,l,m,q} unit;
/// The unit to to use for the force
  enum{d,eng} funit;
/// This holds the pointer that we getting data from
  std::unique_ptr<DataPassingObject> mydata;
protected:
/// Setup the units of the input value
  void setUnit( const std::string& unitstr, const std::string& funitstr );
public:
  static void registerKeywords(Keywords& keys);
  explicit ActionToPutData(const ActionOptions&ao);
/// This resets the stride in the collection object
  void setStride( const unsigned& sss );
/// Check if the value has been set
  bool hasBeenSet() const ;
/// Override clear the input data 
  void clearDerivatives( const bool& force ){}
/// Override the need to deal with gradients
  void setGradientsIfNeeded() override {}
/// Update the units on the input data
  void updateUnits();
/// Do not add chains to setup actions
  bool canChainFromThisAction() const { return false; }
/// The number of derivatives
  unsigned getNumberOfDerivatives() const { return 0; }
/// Do we need to broadcast to all domains
  bool bcastToDomains() const ;
/// Is this quantity scattered over the domains
  bool collectFromDomains() const;
/// Do we always need to collect the atoms from all domains
  bool collectAllFromDomains() const;
/// Set the memory that holds the value
  void set_value(void* val );
/// Set the memory that holds the force
  void set_force(void* val );
/// Share the data from the holder when the data is distributed over domains
  void share( const unsigned& j, const unsigned& k );
  void share( const std::set<AtomNumber>&index, const std::vector<unsigned>& i );
/// Get the data to share
  virtual void wait();
/// Actually set the values for the output
  void calculate(){ firststep=false; wasscaled=false; }
  virtual void apply();
  void rescaleForces( const double& alpha );
/// For replica exchange
  void writeBinary(std::ostream&o);
  virtual void readBinary(std::istream&i);
};

inline
bool ActionToPutData::hasBeenSet() const {
  return wasset;
}

inline
bool ActionToPutData::collectFromDomains() const {
  if( scattered && fixed ) return firststep;
  return scattered;
}

inline
bool ActionToPutData::collectAllFromDomains() const {
  if( scattered && fixed ) return firststep;
  return false;
}

}
#endif
