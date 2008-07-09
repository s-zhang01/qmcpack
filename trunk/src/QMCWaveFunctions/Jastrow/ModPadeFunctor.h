//////////////////////////////////////////////////////////////////
// (c) Copyright 2003-  by Jeongnim Kim
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
#ifndef QMCPLUSPLUS_MODIFIED_PADEFUNCTION_H
#define QMCPLUSPLUS_MODIFIED_PADEFUNCTION_H

#include "Numerics/OptimizableFunctorBase.h"

namespace qmcplusplus {
  /** ModPade Jastrow functional
   *
   * \f[ u(r) = \frac{1}{2*A}\left[1-\exp(-A*r)\right], \f]
   */
  template<class T>
    struct ModPadeFunctor: public OptimizableFunctorBase<T> 
    {

      typedef typename OptimizableFunctorBase<T>::real_type real_type;
      typedef typename OptimizableFunctorBase<T>::OptimizableSetType OptimizableSetType;

      ///coefficients
      real_type A;
      real_type B;
      real_type Zeff;

      real_type Coeff;
      real_type mAB;

      string ID_B;

      /** constructor
       * @param a A coefficient
       * @param samespin boolean to indicate if this function is for parallel spins
       */
      ModPadeFunctor(real_type a=-0.5, real_type b=1): Zeff(1.0) 
      {
        reset(a,b);
      }


      /** reset the internal variables.
       *@param a New Jastrow parameter a 
       */
      inline void reset(real_type a, real_type b) {
        A = a;
        B = b;
        Coeff=A/B;
        mAB = -A*B;
      }

      /** evaluate the value at r
       * @param r the distance
       * @return \f$ u(r) = \frac{A}{r}\left[1-\exp(-\frac{r}{F})\right]\f$
       */
      inline real_type evaluate(real_type r) {
        return Coeff*(1.0-std::exp(-B*r));
      }

      /**@param r the distance
        @param dudr first derivative
        @param d2udr second derivative
        @return the value
        */
      inline real_type evaluate(real_type r, real_type& dudr, real_type& d2udr2) {
        real_type expmar=std::exp(-B*r);
        dudr=A*expmar;
        d2udr2=mAB*expmar;
        return Coeff*(1.0-expmar);
      }

      /** return a value at r
      */
      real_type f(real_type r) {
        return evaluate(r);
      }

      /** return a derivative at r
      */
      real_type df(real_type r) {
        return A*std::exp(-B*r);
      }

      /** Read in the parameter from the xml input file.
       * @param cur current xmlNode from which the data members are reset
       */
      bool put(xmlNodePtr cur){
        Zeff=1.0;
        //jastrow[iab]->put(cur->xmlChildrenNode,wfs_ref.RealVars);
        xmlNodePtr tcur = cur->xmlChildrenNode;
        while(tcur != NULL) {
          //@todo Var -> <param(eter) role="opt"/>
          string cname((const char*)(tcur->name));
          if(cname == "parameter" || cname == "Var") {
            string aname((const char*)(xmlGetProp(tcur,(const xmlChar *)"name")));
            string idname((const char*)(xmlGetProp(tcur,(const xmlChar *)"id")));
            if(aname == "A") {
              putContent(A,tcur);
            } else if(aname == "B") {
              ID_B = idname;
              putContent(B,tcur);
            } else if(aname == "Z") {
              putContent(Zeff,tcur);
            }
          }
          tcur = tcur->next;
        }
        reset(A,B);
        return true;
      }

      /** add A as an optimizable variable
       * @param vlist VarRegistry<T1> to which the variable A will be added for optimization
       */
      void addOptimizables(OptimizableSetType& vlist) 
      {
        vlist[ID_B]=B;
      }

      /** reset the internal variables.
       *
       * USE_resetParameters
       */
      inline void resetParameters(OptimizableSetType& optVariables) 
      {
        typename OptimizableSetType::iterator it(optVariables.find(ID_B));
        if(it != optVariables.end())
        {
          B=(*it).second;
          Coeff=A/B;
          mAB = -A*B;
        }
      }

    };
}
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/

