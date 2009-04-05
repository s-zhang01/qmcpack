//////////////////////////////////////////////////////////////////
// (c) Copyright 1998-2002,2003- by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   Jeongnim Kim
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: jnkim@ncsa.uiuc.edu
//
// Supported by 
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
// -*- C++ -*-

#include "QMCWaveFunctions/Fermion/MultiDiracDeterminantBase.h"
#include "Numerics/DeterminantOperators.h"
#include "Numerics/OhmmsBlas.h"
#include "Numerics/MatrixOperators.h"

namespace qmcplusplus {

  /** set the index of the first particle in the determinant and reset the size of the determinant
   *@param first index of first particle
   *@param nel number of particles in the determinant
   */
  void MultiDiracDeterminantBase::set(int first, int nel) {
    FirstIndex = first;
    resize(nel,nel);
  }



  void MultiDiracDeterminantBase::set_Multi(int first, int nel,int norb) {
    FirstIndex = first;
    resize(nel,norb);
  }


  MultiDiracDeterminantBase::RealType MultiDiracDeterminantBase::updateBuffer(ParticleSet& P, 
      PooledData<RealType>& buf, bool fromscratch) 
  {
    ///needs to be fixed to work with tranposed psiM
    myG=0.0;
    myL=0.0;

    if(fromscratch)
      LogValue=evaluateLog(P,myG,myL);
    else
    {
      if(UpdateMode == ORB_PBYP_RATIO) 
	Phi->evaluate(P, FirstIndex, LastIndex, psiM_temp,dpsiM, d2psiM);    

      if(NumPtcls==1) {
	APP_ABORT("Evaluate Log with 1 particle in MultiMultiDiracDeterminantBase is potentially dangerous");
        ValueType y=1.0/psiM_temp(0,0);
        psiM(0,0)=y;
        GradType rv = y*dpsiM(0,0);
        myG(FirstIndex) += rv;
        myL(FirstIndex) += y*d2psiM(0,0) - dot(rv,rv);
      } else {
        const ValueType* restrict yptr=psiM.data();
        const ValueType* restrict d2yptr=d2psiM.data();
        const GradType* restrict dyptr=dpsiM.data();
        for(int i=0, iat=FirstIndex; i<NumPtcls; i++, iat++) 
        {
          GradType rv;
          ValueType lap=0.0;
          for(int j=0; j<NumOrbitals; j++,yptr++) {
            rv += *yptr * *dyptr++;
            lap += *yptr * *d2yptr++;
          }
          myG(iat) += rv;
          myL(iat) += lap - dot(rv,rv);
        }
      }
    }

    P.G += myG;
    P.L += myL;

    buf.put(psiM.first_address(),psiM.last_address());
    buf.put(psiM_actual.first_address(),psiM_actual.last_address());
    buf.put(FirstAddressOfdV,LastAddressOfdV);
    buf.put(d2psiM.first_address(),d2psiM.last_address());
    buf.put(myL.first_address(), myL.last_address());
    buf.put(FirstAddressOfG,LastAddressOfG);
    buf.put(LogValue);
    buf.put(PhaseValue);

    return LogValue;
  }

  void MultiDiracDeterminantBase::copyFromBuffer(ParticleSet& P, PooledData<RealType>& buf) {

    buf.get(psiM.first_address(),psiM.last_address());
    buf.get(psiM_actual.first_address(),psiM_actual.last_address());
    buf.get(FirstAddressOfdV,LastAddressOfdV);
    buf.get(d2psiM.first_address(),d2psiM.last_address());
    buf.get(myL.first_address(), myL.last_address());
    buf.get(FirstAddressOfG,LastAddressOfG);
    buf.get(LogValue);
    buf.get(PhaseValue);

    //re-evaluate it for testing
    //Phi.evaluate(P, FirstIndex, LastIndex, psiM, dpsiM, d2psiM);
    //CurrentDet = Invert(psiM.data(),NumPtcls,NumOrbitals);
    //need extra copy for gradient/laplacian calculations without updating it
    psiM_temp = psiM;
    dpsiM_temp = dpsiM;
    d2psiM_temp = d2psiM;
  }
  


  MultiDiracDeterminantBase::GradType 
    MultiDiracDeterminantBase::evalGrad(ParticleSet& P, int iat)
  {
    cerr<<"GS VALUE PSI init IS "<<iat<<" "<<gs_value_psi<<endl;

    Phi->evaluate(P, FirstIndex, LastIndex, psiM_actual,dpsiM, d2psiM);
    //     const ValueType* restrict yptr=psiM[iat-FirstIndex];
    //     const GradType* restrict dyptr=dpsiM[iat-FirstIndex];
     GradType rv;
     //     for(int j=0; j<NumOrbitals; ++j) rv += (*yptr++) *(*dyptr++);
     for(int j=0; j<NumOrbitals; j++) 
       rv += dpsiM(iat-FirstIndex,j)*psiM(iat-FirstIndex,j); //   (*yptr++) *(*dyptr++);

     //     int dim=0;

//      //attempt 1
//      for (int i=0;i<NumOrbitals; i++)
//        psiV[i]=dpsiM(iat,i)[dim];
//      DetUpdate(psiM,psiV,workV1,workV2,iat-FirstIndex,rv[0]);
//      for (int i=0;i<psiM.extent(0);i++){
//        for (int j=0;j<psiM.extent(1);j++){
// 	 cerr<<psiM(i,j)<<endl;
//        }
//      }
//      assert(1==2);

     ////attempt 2


     GradVector_t workV2_grad(workV2.size());
     for (int dim=0;dim<OHMMS_DIM;dim++){
       for (int i=0;i<NumOrbitals; i++){
	 workV1[i]=-(dpsiM(iat-FirstIndex,i)[dim]-psiM_actual(i,iat-FirstIndex));
       }
       //really I think we only need to take a piece of this product!
       MatrixOperators::product(psiM,workV1,workV2.data()); //check to see if this doesn't need a transpose on the psiM
       for (int i=0;i<NumOrbitals;i++)
	 workV2_grad(i)[dim]=workV2(i);
     }
     

     psiMInv=psiM;
     for (int i=0;i<psiMInv.extent(0);i++)
       for (int j=0;j<psiMInv.extent(1);j++){
	 psiMInv(i,j)+=(1.0/rv[2])*workV2[i]*psiM(iat-FirstIndex,j);
	 cerr<<psiMInv(i,j)<<endl;
       }
     

  //  GradType one_over_ratio=1.0/rv;
  Excitations.BuildDotProducts_grad(psiM,psiM_actual,dpsiM,workV2_grad,1.0/rv,iat-FirstIndex);
  GradType val(1.0,1.0,1.0);
  int coefIndex=0;
  Excitations.CalcSingleExcitations_grad(coefs,val,coefIndex);
  Excitations.CalcDoubleExcitations_grad(coefs,val,coefIndex);
  cerr<<"GS VALUE PSI IS "<<gs_value_psi<<" "<<rv<<" "<<val<<endl;
  //  assert(1==2);
  return val*rv*gs_value_psi; //*gs_value_psi;
     //     Excitations.BuildDotProducts(psiM,workV1,workV2,dpsiM_actual,1.0/rv);
//     double coefIndex=0;
//     ValueType newVal=1.0;
//     Excitations.CalcSingleExcitations(coefs,newVal,coefIndex);
//     cerr<<(rv*newVal)/psi_old;
//     return rv;
  

//   MultiDiracDeterminantBase::GradType 
//     MultiDiracDeterminantBase::evalGrad_slow(ParticleSet& P, int iat)
//   {
//     const ValueType* restrict yptr=psiM[iat-FirstIndex];
//     const GradType* restrict dyptr=dpsiM[iat-FirstIndex];
//     GradType rv;
//     for(int j=0; j<NumOrbitals; ++j) rv += (*yptr++) *(*dyptr++);
//     psiMInv=psiM_temp;
//     DetUpdate(psiMInv,dpsiM[iat-FirstIndex],workV1,workV2,iat-FirstIndex,rv);    
//     TinyVector<int,2> second_replaces_first=Excitations.begin();
//     while (second_replaces_first[0]!=-1){
      
      

    

//     }

//     return rv;
//   }


  }
    MultiDiracDeterminantBase::ValueType 
      MultiDiracDeterminantBase::ratioGrad(ParticleSet& P, int iat, GradType& grad_iat)
  {
    Phi->evaluate(P, iat, psiV, dpsiV, d2psiV);
    WorkingIndex = iat-FirstIndex;

    UpdateMode=ORB_PBYP_PARTIAL;
    
    const ValueType* restrict vptr = psiV.data();
    const ValueType* restrict yptr = psiM[WorkingIndex];
    const GradType* restrict dyptr = dpsiV.data();
    
    GradType rv;
    curRatio = 0.0;
    for(int j=0; j<NumOrbitals; ++j) {
      rv       += yptr[j] * dyptr[j];
      curRatio += yptr[j] * vptr[j];
    }
    grad_iat += (1.0/curRatio) * rv;
    return curRatio;
  }

  /** return the ratio
   * @param P current configuration
   * @param iat particle whose position is moved
   * @param dG differential Gradients
   * @param dL differential Laplacians
   *
   * Data member *_temp contain the data assuming that the move is accepted
   * and are used to evaluate differential Gradients and Laplacians.
   */
  MultiDiracDeterminantBase::ValueType MultiDiracDeterminantBase::ratio(ParticleSet& P, int iat,
      ParticleSet::ParticleGradient_t& dG, 
      ParticleSet::ParticleLaplacian_t& dL) 
  {
    APP_ABORT("  MultiDiracDeterminantBase::ratio with grad is distabled");
    UpdateMode=ORB_PBYP_ALL;
    Phi->evaluate(P, iat, psiV, dpsiV, d2psiV);
    WorkingIndex = iat-FirstIndex;

    //psiM_temp = psiM;
#ifdef DIRAC_USE_BLAS
    curRatio = BLAS::dot(NumOrbitals,psiM_temp[WorkingIndex],&psiV[0]);
#else
    curRatio= DetRatio(psiM_temp, psiV.begin(),WorkingIndex);
#endif

    if(abs(curRatio)<numeric_limits<RealType>::epsilon()) 
    {
      UpdateMode=ORB_PBYP_RATIO; //singularity! do not update inverse 
      return 0.0;
    }

    //update psiM_temp with the row substituted
    DetUpdate(psiM_temp,psiV,workV1,workV2,WorkingIndex,curRatio);

    //update dpsiM_temp and d2psiM_temp 
    for(int j=0; j<NumOrbitals; j++) {
      dpsiM_temp(WorkingIndex,j)=dpsiV[j];
      d2psiM_temp(WorkingIndex,j)=d2psiV[j];
    }

    int kat=FirstIndex;

    const ValueType* restrict yptr=psiM_temp.data();
    const ValueType* restrict d2yptr=d2psiM_temp.data();
    const GradType* restrict dyptr=dpsiM_temp.data();
    for(int i=0; i<NumPtcls; i++,kat++) {
      //This mimics gemm with loop optimization
      GradType rv;
      ValueType lap=0.0;
      for(int j=0; j<NumOrbitals; j++,yptr++) {
        rv += *yptr * *dyptr++;
        lap += *yptr * *d2yptr++;
      }

      //using inline dot functions
      //GradType rv=dot(psiM_temp[i],dpsiM_temp[i],NumOrbitals);
      //ValueType lap=dot(psiM_temp[i],d2psiM_temp[i],NumOrbitals);

      //Old index: This is not pretty
      //GradType rv =psiM_temp(i,0)*dpsiM_temp(i,0);
      //ValueType lap=psiM_temp(i,0)*d2psiM_temp(i,0);
      //for(int j=1; j<NumOrbitals; j++) {
      //  rv += psiM_temp(i,j)*dpsiM_temp(i,j);
      //  lap += psiM_temp(i,j)*d2psiM_temp(i,j);
      //}
      lap -= dot(rv,rv);
      dG[kat] += rv - myG[kat];  myG_temp[kat]=rv;
      dL[kat] += lap -myL[kat];  myL_temp[kat]=lap;
    }

    return curRatio;
  }


  /** return the ratio only for the  iat-th partcle move
   * @param P current configuration
   * @param iat the particle thas is being moved
   * psiM and psiM_temp shoudl be the same upon entering
   */
  MultiDiracDeterminantBase::ValueType MultiDiracDeterminantBase::ratio(ParticleSet& P, int iat)
  {
    old_gs_value_psi=gs_value_psi;
    UpdateMode=ORB_PBYP_ALL;
    WorkingIndex = iat-FirstIndex;
    Phi->evaluate(P, iat, psiV);


// 	///Copying ground state data to psiM
// 	for (int orbital=0;orbital<NumOrbitals;orbital++)
// 	  for (int ptcl=0;ptcl<NumPtcls;ptcl++) 
// 	    psiM_temp(orbital,ptcl)=psiM_actual(orbital,ptcl); //Note: Because psiM is typically an inverse, it's typically ptcl,orbital
// 	//done copying
// 	LogValue=InvertWithLog(psiM_temp.data(),NumPtcls,NumOrbitals,WorkSpace.data(),Pivot.data(),PhaseValue);


    psiMInv=psiM_temp;
    MatrixOperators::transpose(psiMInv);
    Excitations.BuildDotProducts(psiMInv,psiM_actual);
    ValueType oldVal=1.0; //eventually store previous valuess
    int coefIndex=0;
    Excitations.CalcSingleExcitations(coefs,oldVal,coefIndex);
    Excitations.CalcDoubleExcitations(coefs,oldVal,coefIndex);
    ValueType gs_ratio=0.0;
    for (int orbital=0;orbital<NumOrbitals;orbital++)
      gs_ratio+=psiM_temp(WorkingIndex,orbital)*psiV(orbital);
    gs_value_psi*=gs_ratio;


    //relying here on the fact that DetUpdate only uses the first (psiM_temp.cols()) rows of psiV!
    //update psiM_temp with the row substituted


    for (int orbital=0;orbital<NumOrbitals_total;orbital++){
      psiV_old(orbital)=psiM_actual(orbital,WorkingIndex);
      psiM_actual(orbital,WorkingIndex)=psiV(orbital);
    }

    DetUpdate(psiM_temp,psiV,workV1,workV2,WorkingIndex,gs_ratio);


// 	///Copying ground state data to psiM
// 	for (int orbital=0;orbital<NumOrbitals;orbital++)
// 	  for (int ptcl=0;ptcl<NumPtcls;ptcl++) 
// 	    psiM_temp(orbital,ptcl)=psiM_actual(orbital,ptcl); //Note: Because psiM is typically an inverse, it's typically ptcl,orbital
// 	//done copying
// 	LogValue=InvertWithLog(psiM_temp.data(),NumPtcls,NumOrbitals,WorkSpace.data(),Pivot.data(),PhaseValue);



    psiMInv=psiM_temp;
    MatrixOperators::transpose(psiMInv);
   //currnetly psiM sending incorrectly transposed!
    Excitations.BuildDotProducts(psiMInv,psiM_actual);
    ValueType val=1.0;
    coefIndex=0;
    Excitations.CalcSingleExcitations(coefs,val,coefIndex);
    Excitations.CalcDoubleExcitations(coefs,val,coefIndex);
    curRatio=(gs_ratio*val)/oldVal;
    gs_value_psi=gs_value_psi/curRatio;
    psi_new=val;
    psi_old=oldVal;
    return curRatio;
  }





  /** move was accepted, update the real container
  */
  void MultiDiracDeterminantBase::acceptMove(ParticleSet& P, int iat) 
  {
    cerr<<"MultiDiracDeterminantBase accepting "<<iat<<" "<<FirstIndex<<endl;
    psi_new=psi_old;
    PhaseValue += evaluatePhase(curRatio);
    LogValue +=std::log(std::abs(curRatio));
    switch(UpdateMode)
    {
      case ORB_PBYP_RATIO:
        DetUpdate(psiM,psiV,workV1,workV2,WorkingIndex,curRatio);
        break;
      case ORB_PBYP_PARTIAL:
        //psiM = psiM_temp;
	DetUpdate(psiM,psiV,workV1,workV2,WorkingIndex,curRatio);
        std::copy(dpsiV.begin(),dpsiV.end(),dpsiM[WorkingIndex]);
        std::copy(d2psiV.begin(),d2psiV.end(),d2psiM[WorkingIndex]);

        //////////////////////////////////////
        ////THIS WILL BE REMOVED. ONLY FOR DEBUG DUE TO WAVEFUNCTIONTEST
        //myG = myG_temp;
        //myL = myL_temp;
        ///////////////////////

        break;
      default:
        myG = myG_temp;
        myL = myL_temp;
        psiM = psiM_temp;
        std::copy(dpsiV.begin(),dpsiV.end(),dpsiM[WorkingIndex]);
        std::copy(d2psiV.begin(),d2psiV.end(),d2psiM[WorkingIndex]);
        break;
    }

    curRatio=1.0;
  }

  /** move was rejected. copy the real container to the temporary to move on
  */
  void MultiDiracDeterminantBase::restore(int iat) {
    gs_value_psi=old_gs_value_psi;
    psi_old=psi_new;
    if(UpdateMode == ORB_PBYP_ALL) {
      psiM_temp = psiM;
      for (int orbital=0;orbital<NumOrbitals_total;orbital++)
	psiM_actual(orbital,WorkingIndex)=psiV_old(orbital);
      std::copy(dpsiM[WorkingIndex],dpsiM[WorkingIndex+1],dpsiM_temp[WorkingIndex]);
      std::copy(d2psiM[WorkingIndex],d2psiM[WorkingIndex+1],d2psiM_temp[WorkingIndex]);
    }
    else if (UpdateMode== ORB_PBYP_RATIO){
      for (int orbital=0;orbital<NumOrbitals_total;orbital++)
	psiM_actual(orbital,WorkingIndex)=psiV_old(orbital);
    }
    curRatio=1.0;
  }


  MultiDiracDeterminantBase::RealType
    MultiDiracDeterminantBase::evaluateLog(ParticleSet& P, 
        ParticleSet::ParticleGradient_t& G, 
        ParticleSet::ParticleLaplacian_t& L)
    {
      //      Phi->evaluate(P, FirstIndex, LastIndex, psiM,dpsiM, d2psiM);
      Phi->evaluate(P, FirstIndex, LastIndex, psiM_actual,dpsiM, d2psiM);

      if(NumPtcls==1) 
      {
	APP_ABORT("Evaluate Log with 1 particle in MultiMultiDiracDeterminantBase is potentially dangerous");
        //CurrentDet=psiM(0,0);
        ValueType det=psiM(0,0);
        ValueType y=1.0/det;
        psiM(0,0)=y;
        GradType rv = y*dpsiM(0,0);
        G(FirstIndex) += rv;
        L(FirstIndex) += y*d2psiM(0,0) - dot(rv,rv);
        LogValue = evaluateLogAndPhase(det,PhaseValue);
      } else {
	
	///Copying ground state data to psiM
	for (int orbital=0;orbital<NumOrbitals;orbital++)
	  for (int ptcl=0;ptcl<NumPtcls;ptcl++) 
	    psiM(orbital,ptcl)=psiM_actual(orbital,ptcl); //Note: Because psiM is typically an inverse, it's typically ptcl,orbital
	//done copying
	LogValue=InvertWithLog(psiM.data(),NumPtcls,NumOrbitals,WorkSpace.data(),Pivot.data(),PhaseValue);
        const ValueType* restrict yptr=psiM.data();
        const ValueType* restrict d2yptr=d2psiM.data();
        const GradType* restrict dyptr=dpsiM.data();
        for(int i=0, iat=FirstIndex; i<NumPtcls; i++, iat++) {
          GradType rv;
          ValueType lap=0.0;
          for(int j=0; j<NumOrbitals; j++,yptr++) {
            rv += *yptr * *dyptr++;
            lap += *yptr * *d2yptr++;
          }
          G(iat) += rv;
          L(iat) += lap - dot(rv,rv);
        }
      }
      psiM_temp = psiM;
      double val1=std::exp(LogValue)*std::cos(abs(PhaseValue));
      //      gs_value_psi=1.0;
      //      gs_value_psi=val1;
      cerr<<"Starting gs_value_psi as "<<gs_value_psi<<endl;
      //      return LogValue;
      //Single Excitations!
      TinyVector<int,2> second_replaces_first=Excitations.begin();
      while (second_replaces_first[0]!=-1){
	for (int orbital=0;orbital<NumOrbitals;orbital++){
	  for (int ptcl=0;ptcl<NumPtcls;ptcl++){ 
	    psiM(orbital,ptcl)=psiM_actual(orbital,ptcl);
	  }
	}
	for (int ptcl=0;ptcl<NumPtcls;ptcl++){
	  psiM(second_replaces_first[0],ptcl)=psiM_actual(second_replaces_first[1],ptcl);
	}
	double PhaseValuep;
	double LogValuep=InvertWithLog(psiM.data(),NumPtcls,NumOrbitals,WorkSpace.data(),Pivot.data(),PhaseValuep);

	double val2=std::exp(LogValuep)*std::cos(abs(PhaseValuep));
	double val=std::exp(LogValue)*std::cos(abs(PhaseValue))+std::exp(LogValuep)*std::cos(abs(PhaseValuep));
	LogValue=std::log(abs(val));
	if (val>0)
	  PhaseValue=0.0;
	else 
	  PhaseValue=M_PI;
	psiM=psiM_temp;
	second_replaces_first=Excitations.next();
      }


      //double excitations
      
      for (int ii1=0;ii1<Excitations.orbitals_to_replace.size();ii1++){
	for (int ii2=ii1+1;ii2<Excitations.orbitals_to_replace.size();ii2++){
	  for (int jj1=0;jj1<Excitations.unoccupied_orbitals_to_use.size();jj1++){
	    for (int jj2=jj1+1;jj2<Excitations.unoccupied_orbitals_to_use.size();jj2++){
	       
	      int or1=Excitations.orbitals_to_replace[ii1];
	      int or2=Excitations.orbitals_to_replace[ii2];
	      int uo1=Excitations.unoccupied_orbitals_to_use[jj1];
	      int uo2=Excitations.unoccupied_orbitals_to_use[jj2];
	      for (int orbital=0;orbital<NumOrbitals;orbital++){
		for (int ptcl=0;ptcl<NumPtcls;ptcl++){ 
		  psiM(orbital,ptcl)=psiM_actual(orbital,ptcl);
		}
	      }
	      for (int ptcl=0;ptcl<NumPtcls;ptcl++){
		psiM(or1,ptcl)=psiM_actual(uo1,ptcl);
		psiM(or2,ptcl)=psiM_actual(uo2,ptcl);
	      }
	      double PhaseValuep;
	      double LogValuep=InvertWithLog(psiM.data(),NumPtcls,NumOrbitals,WorkSpace.data(),Pivot.data(),PhaseValuep);
	      
	      double val2=std::exp(LogValuep)*std::cos(abs(PhaseValuep));

	      double val=std::exp(LogValue)*std::cos(abs(PhaseValue))+std::exp(LogValuep)*std::cos(abs(PhaseValuep));
	      LogValue=std::log(abs(val));
	      if (val>0)
		PhaseValue=0.0;
	      else 
		PhaseValue=M_PI;

	      psiM=psiM_temp;
	      second_replaces_first=Excitations.next();
	    }
	  }
	}
      }
      //double excitations done!

      cerr<<"Pre-gs-value-psi"<<gs_value_psi<<endl;
      gs_value_psi=val1/(std::exp(LogValue)*std::cos(abs(PhaseValue)));
      cerr<<"Post-gs-value-psi"<<gs_value_psi<<endl;
      return LogValue;

      return LogValue;
    }


#include "MultiDiracDeterminantBase_help.cpp";
}
/***************************************************************************
 * $RCSfile$   $Author: kesler $
 * $Revision: 3582 $   $Date: 2009-02-20 18:07:41 -0600 (Fri, 20 Feb 2009) $
 * $Id: MultiDiracDeterminantBase.cpp 3582 2009-02-21 00:07:41Z kesler $ 
 ***************************************************************************/