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
#include "Optimize/VarList.h"

namespace qmcplusplus {

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
        pi = 4.0*std::atan(1.0);
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
        return 0.5*(1.0+std::cos(pi*(r-R1)*R12));
      }
    };


  /** A numerical functor
   *
   * implements interfaces to be used for Jastrow functions
   * - OneBodyJastrow<NumericalJastrow>
   * - TwoBodyJastrow<NumericalJastrow>
   */
  template <class RT>
    struct NumericalJastrow: public JastrowFunctorBase<RT> {

      ///typedef of the source functor
      typedef JastrowFunctorBase<RT> FNIN;
      ///typedef for the argument
      typedef typename FNIN::real_type real_type;
      ///typedef of the target functor
      typedef OneDimCubicSpline<real_type,real_type>  FNOUT;

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
        const typename FNOUT::grid_type& grid = OutFunc->grid();
        //set cutoff function
        int last=grid.size()-1;
        for(int i=0; i<grid.size(); i++) {
          (*OutFunc)(i) = InFunc->f(grid(i))*Rcut(grid(i));
        }

        //boundary conditions
        real_type deriv1=InFunc->df(grid(0));
        real_type deriv2=0.0;
        OutFunc->spline(0,deriv1,last,deriv2);
      }

      /** evaluate everything: value, first and second derivaties
      */
      inline real_type evaluate(real_type r, real_type& dudr, real_type& d2udr2) {
        return OutFunc->splint(r,dudr,d2udr2);
      }

      /** evaluate value only
      */
      inline real_type evaluate(real_type r) {
        return OutFunc->splint(r);
      }

      /** implement the virtual function of JastrowFunctorBase */
      real_type f(real_type r) {
        return OutFunc->splint(r);
      }

      /** implement the virtual function of JastrowFunctorBase  */
      real_type df(real_type r) {
        real_type dudr,d2udr2;
        OutFunc->splint(r,dudr,d2udr2);
        return dudr;
      }

      void put(xmlNodePtr cur, VarRegistry<real_type>& vlist) {
        InFunc->put(cur,vlist);
      }

      void print(ostream& os) {
        const typename FNOUT::grid_type& grid = OutFunc->grid();
        for(int i=0; i<grid.size(); i++) {
          cout << grid(i) << " " << (*OutFunc)(i) << endl;
        }
      }

      ///set the input, analytic function
      void initialize(FNIN* in_, typename FNOUT::grid_type* agrid, real_type rcut) { 
        InFunc=in_;
        setOutFunc(new FNOUT(agrid));
        setCutoff(rcut,agrid->rmax());
        reset();
      }
    };

  /** A numerical functor
   *
   * implements interfaces to be used for Jastrow functions
   * - OneBodyJastrow<NumericalJastrow>
   * - TwoBodyJastrow<NumericalJastrow>
   */
  template <class RT>
    struct SplineJastrow: public JastrowFunctorBase<RT> {

      ///typedef of the source functor
      typedef JastrowFunctorBase<RT> FNIN;
      ///typedef for the argument
      typedef typename FNIN::real_type real_type;
      ///typedef of the target functor
      typedef OneDimCubicSpline<real_type,real_type>  FNOUT;

      real_type Rmax;
      FNIN *InFunc;
      FNOUT *OutFunc;

      ///constructor
      SplineJastrow(): InFunc(0), OutFunc(0) { }
      ///constructor with arguments
      SplineJastrow(FNIN* in_, typename FNOUT::grid_type* agrid){
        initialize(in_,agrid);
      }
      ///set the input, analytic function
      void setInFunc(FNIN* in_) { InFunc=in_;}
      ///set the output numerical function
      void setOutFunc(FNOUT* out_) { OutFunc=out_;}
      ///reset the input/output function
      inline void reset() {
        InFunc->reset();
        //reference to the output functions grid
        const typename FNOUT::grid_type& grid = OutFunc->grid();
        //set cutoff function
        int last=grid.size()-1;
        for(int i=0; i<grid.size(); i++) {
          (*OutFunc)(i) = InFunc->f(grid(i));
//	  cout << grid(i) << "   " << (*OutFunc)(i) << endl;
        }
	(*OutFunc)(last)=0.0;
        //boundary conditions
        real_type deriv1=InFunc->df(grid(0));
        real_type deriv2=0.0;
        OutFunc->spline(0,deriv1,last,deriv2);
        Rmax=grid(last);
      }

      /** evaluate everything: value, first and second derivaties
      */
      inline real_type evaluate(real_type r, real_type& dudr, real_type& d2udr2) {
        return OutFunc->splint(r,dudr,d2udr2);
      }

      /** evaluate value only
      */
      inline real_type evaluate(real_type r) {
        return OutFunc->splint(r);
      }

      /** implement the virtual function of JastrowFunctorBase */
      real_type f(real_type r) {
        return OutFunc->splint(r);
      }

      /** implement the virtual function of JastrowFunctorBase  */
      real_type df(real_type r) {
        real_type dudr,d2udr2;
        OutFunc->splint(r,dudr,d2udr2);
        return dudr;
      }

      void put(xmlNodePtr cur, VarRegistry<real_type>& vlist) {
        InFunc->put(cur,vlist);
      }

      void print(ostream& os) {
        const typename FNOUT::grid_type& grid = OutFunc->grid();
        for(int i=0; i<grid.size(); i++) {
          cout << grid(i) << " " << (*OutFunc)(i) << endl;
        }
      }

      ///set the input, analytic function
      void initialize(FNIN* in_, typename FNOUT::grid_type* agrid) { 
        InFunc=in_;
        setOutFunc(new FNOUT(agrid));
        reset();
      }
    };

}
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
