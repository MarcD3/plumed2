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
#include "DataPassingObject.h"
#include "tools/OpenMP.h"
#include "tools/Tools.h"

namespace PLMD {

template<typename T>
static void getPointer(const TypesafePtr & p, const std::vector<unsigned>& shape, const unsigned& start, const unsigned& stride, T*&pp ) {
  if( shape.size()==1 && stride==1 ) { pp=p.get<T*>( shape[0] );  }
  else if( shape.size()==1 ) { auto p_=p.get<T*>( {shape[0],stride} ); pp = p_+start; }
  else if( shape.size()==2 ) { pp=p.get<T*>( {shape[0],shape[1]} ); }
} 

template <class T>
class DataPassingObjectTyped : public DataPassingObject {
private:
/// A pointer to the value
  TypesafePtr v;
/// A pointer to the force 
  TypesafePtr f;
public:
/// This convers a number from the MD code into a double
  double MD2double(const TypesafePtr &) const override ;
/// This is used when you want to save the passed object to a double variable in PLUMED rather than the pointer
/// this can be used even when you don't pass a pointer from the MD code
  void saveValueAsDouble( const TypesafePtr & val ) override;
/// Set the pointer to the value
  void setValuePointer( const TypesafePtr & p, const std::vector<unsigned>& shape, const bool& isconst ) override;
/// Set the pointer to the force
  void setForcePointer( const TypesafePtr & p, const std::vector<unsigned>& shape ) override;
/// Share the data and put it in the value from sequential data
  void share_data( const unsigned& j, const unsigned& k, Value* value ) override;
/// Share the data and put it in the value from a scattered data
  void share_data( const std::set<AtomNumber>&index, const std::vector<unsigned>& i, Value* value ) override;
/// Pass the force from the value to the output value
  void add_force( Value* vv ) override;
  void add_force( const std::vector<int>& index, Value* value ) override;
  void add_force( const std::set<AtomNumber>& index, const std::vector<unsigned>& i, Value* value ) override;
/// Rescale the force on the output value
  void rescale_force( const unsigned& n, const double& factor, Value* value ) override;
/// This transfers everything to the output
  void setData( Value* value ) override;
};

std::unique_ptr<DataPassingObject> DataPassingObject::create(unsigned n) {
  if(n==sizeof(double)) {
    return std::unique_ptr<DataPassingObject>(new DataPassingObjectTyped<double>());
  } else  if(n==sizeof(float)) {
    return std::unique_ptr<DataPassingObject>(new DataPassingObjectTyped<float>());
  }
  std::string pp; Tools::convert(n,pp);
  plumed_merror("cannot create an MD interface with sizeof(real)=="+ pp);
  return NULL;
}

template <class T>
double DataPassingObjectTyped<T>::MD2double(const TypesafePtr & m) const {
  double d=double(m.template get<T>()); return d;
}

template <class T>
void DataPassingObjectTyped<T>::saveValueAsDouble( const TypesafePtr & val ) {
  hasbackup=true; bvalue=double(val.template get<T>());
  // The following is to avoid extra digits in case the MD code uses floats
  // e.g.: float f=0.002 when converted to double becomes 0.002000000094995
  // To avoid this, we keep only up to 6 significant digits after first one
  if( sizeof(T)<=4) {
      double magnitude=std::pow(10,std::floor(std::log10(bvalue)));
      bvalue=std::round(bvalue/magnitude*1e6)/1e6*magnitude;
  }
}

template <class T>
void DataPassingObjectTyped<T>::setValuePointer( const TypesafePtr & val, const std::vector<unsigned>& shape, const bool& isconst ) {
   if( shape.size()==0 ) {
       if( isconst ) val.get<const T*>(); else val.get<T*>();  // just check type and discard pointer
   } else if( shape.size()==1 ) {
       if( isconst ) val.get<const T*>(shape[0]); else val.get<T*>(shape[0]);  // just check type and discard pointer 
   } else if( shape.size()==2 ) {
       if( isconst ) val.get<const T*>({shape[0],shape[1]}); else val.get<T*>({shape[0],shape[1]});  // just check type and discard pointer 
   }
   v=val.copy();
}

template <class T>
void DataPassingObjectTyped<T>::setForcePointer( const TypesafePtr & val, const std::vector<unsigned>& shape ) {
   if( shape.size()==0 ) val.get<T*>();  // just check type and discard pointer
   else if( shape.size()==1 ) val.get<T*>(shape[0]);   // just check type and discard pointer
   else if( shape.size()==2 ) val.get<T*>({shape[0],shape[1]});   // just check type and discard pointer
   f=val.copy(); 
}

template <class T>
void DataPassingObjectTyped<T>::setData( Value* value ) {
   if( value->getRank()==0 ) { *v.template get<T*>() = static_cast<T>(value->get()) / unit; return; }
   T* pp; getPointer( v, value->getShape(), start, stride, pp ); unsigned nvals=value->getNumberOfValues();
   for(unsigned i=0;i<nvals;++i) pp[i] = T( value->get(i) );
}

template <class T>
void DataPassingObjectTyped<T>::share_data( const unsigned& j, const unsigned& k, Value* value ) {
  if( value->getRank()==0 ) { 
      if( hasbackup ) value->set( 0, unit*bvalue );
      else value->set( 0, unit*double(v.template get<T>()) ); 
      return; 
  }
  std::vector<unsigned> s(value->getShape()); if( s.size()==1 ) s[0]=k-j;
  const T* pp; getPointer( v, s, start, stride, pp );
  #pragma omp parallel for num_threads(value->getGoodNumThreads(j,k))
  for(unsigned i=j; i<k; ++i) { value->set( i, unit*pp[i*stride] ); }
}

template <class T>
void DataPassingObjectTyped<T>::share_data( const std::set<AtomNumber>&index, const std::vector<unsigned>& i, Value* value ) {
  plumed_assert( value->getRank()==1 ); std::vector<unsigned> s(1,index.size()); const T* pp; getPointer( v, s, start, stride, pp );
  // cannot be parallelized with omp because access to data is not ordered
  unsigned k=0; for(const auto & p : index) { value->set( p.index(), unit*pp[i[k]*stride] ); k++; }
}

template <class T>
void DataPassingObjectTyped<T>::add_force( Value* value ) {
   if( value->getRank()==0 ) { *f.template get<T*>() += funit*static_cast<T>(value->getForce(0)); return; }
   T* pp; getPointer( f, value->getShape(), start, stride, pp ); unsigned nvals=value->getNumberOfValues();
   #pragma omp parallel for num_threads(OpenMP::getGoodNumThreads(pp,nvals))
   for(unsigned i=0;i<nvals;++i) pp[i*stride] += funit*T(value->getForce(i));
}

template <class T>
void DataPassingObjectTyped<T>::add_force( const std::vector<int>& index, Value* value ) {
   plumed_assert( value->getRank()==1 ); std::vector<unsigned> s(1,index.size()); T* pp; getPointer( f, s, start, stride, pp ); 
   #pragma omp parallel for num_threads(OpenMP::getGoodNumThreads(pp,index.size()))
   for(unsigned i=0; i<index.size(); ++i) pp[i*stride] += funit*T(value->getForce(index[i])); 
}

template <class T>
void DataPassingObjectTyped<T>::add_force( const std::set<AtomNumber>& index, const std::vector<unsigned>& i, Value* value ) {
   plumed_assert( value->getRank()==1 ); std::vector<unsigned> s(1,index.size()); T* pp; getPointer( f, s, start, stride, pp );
   unsigned k=0; for(const auto & p : index) { pp[stride*i[k]] += funit*T(value->getForce(p.index())); k++; }
}

template <class T>
void DataPassingObjectTyped<T>::rescale_force( const unsigned& n, const double& factor, Value* value ) {
   plumed_assert( value->getRank()>0 ); std::vector<unsigned> s( value->getShape() ); if( s.size()==1 ) s[0] = n;
   T* pp; getPointer( f, s, start, stride, pp );
   #pragma omp parallel for num_threads(OpenMP::getGoodNumThreads(pp,n))
   for(unsigned i=0;i<n;++i) pp[i*stride] *= T(factor);
}
 
}
