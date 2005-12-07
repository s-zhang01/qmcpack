//////////////////////////////////////////////////////////////////
// (c) Copyright 2003  by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: jnkim@ncsa.uiuc.edu
//   Tel:    217-244-6319 (NCSA) 217-333-3324 (MCC)
//
// Supported by 
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#ifndef QMCPLUSPLUS_NUMERICAL_JASTROWFUNCTIONS_H
#define QMCPLUSPLUS_NUMERICAL_JASTROWFUNCTIONS_H
#include "QMCWaveFunctions/JastrowFunctorBase.h"
#include "Numerics/OneDimCubicSpline.h"

template<class T>
struct CutoffFunctor {
  T R1;
  T R2;
  T R12;
  T pi;
  CutoffFunctor() {}
  inline CutoffFunctor(T r1, T r2){
    set(r1,r2);
  }
  inline void set(T r1, T r2) {
    pi = 4.0*atan(1.0);
    R1=r1; 
    if(r2<=r1) { 
      R2=R1; R12=1e9;
    } else {
      R2=r2;R12=1.0/(R2-R1);
    }
  }
  inline T operator()(T r) {
    if(r<R1) return 1.0;
    if(r>R2) return 0.0;
    return 0.5*(1.0+cos(pi*(r-R1)*R12));
  }
};
/** A nuerical functor
 *
 * implements interfaces to be used for Jastrow functions
 * - OneBodyJastrow<NumericalJastrow>
 * - TwoBodyJastrow<NumericalJastrow>
 */
template <class RT>
struct NumericalJastrow {

  ///typedef of the source functor
  typedef JastrowFunctorBase<RT> FNIN;
  ///typedef for the argument
  typedef typename FNIN::real_type real_type;
  ///typedef for the return value
  typedef typename FNIN::value_type value_type;
  ///typedef of the target functor
  //typedef OneDimGridFunctor<value_type,real_type>  FNOUT;
  typedef OneDimCubicSpline<value_type,real_type>  FNOUT;

  FNIN *InFunc;
  FNOUT *OutFunc;
  CutoffFunctor<real_type> Rcut;

  ///constrctor
  NumericalJastrow(): InFunc(0), OutFunc(0) { }
  ///set the input, analytic function
  void setInFunc(FNIN* in_) { InFunc=in_;}
  ///set the output numerical function
  void setOutFunc(FNOUT* out_) { OutFunc=out_;}
  ///set the cutoff function
  void setCutoff(real_type r1, real_type r2) {
    Rcut.set(r1,r2);
  }
  ///reset the input/output function
  inline void reset() {
    InFunc->reset();
    //reference to the output functions grid
    const FNOUT::grid_type& grid = OutFunc->grid();
    //set cutoff function
    int last=grid.size()-1;
    for(int i=0; i<grid.size(); i++) {
      (*OutFunc)(i) = InFunc->f(grid(i))*Rcut(grid(i));
    }

    //boundary conditions
    value_type deriv1=InFunc->df(grid(0));
    value_type deriv2=0.0;
    OutFunc->spline(0,deriv1,last,deriv2);
  }

  /** evaluate everything: value, first and second derivaties
   */
  inline value_type evaluate(real_type r, value_type& dudr, value_type& d2udr2) {
    return OutFunc->splint(r,dudr,d2udr2);
  }

  /** evaluate value only
   */
  inline value_type evaluate(real_type r) {
    return OutFunc->splint(r);
  }

  void put(xmlNodePtr cur, VarRegistry<real_type>& vlist) {
    InFunc->put(cur,vlist);
  }

  void print(ostream& os) {
    const FNOUT::grid_type& grid = OutFunc->grid();
    for(int i=0; i<grid.size(); i++) {
      cout << grid(i) << " " << (*OutFunc)(i) << endl;
    }
  }
};
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/

