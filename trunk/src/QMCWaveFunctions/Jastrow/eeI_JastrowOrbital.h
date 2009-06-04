//////////////////////////////////////////////////////////////////
// (c) Copyright 2003-  by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
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
#ifndef QMCPLUSPLUS_COMMON_EEN_JASTROW_H
#define QMCPLUSPLUS_COMMON_EEN_JASTROW_H
#include "Configuration.h"
#include  <map>
#include  <numeric>
#include "QMCWaveFunctions/OrbitalBase.h"
#include "Particle/DistanceTableData.h"
#include "Particle/DistanceTable.h"
#include "LongRange/StructFact.h"

namespace qmcplusplus {

  struct IonData
  {
    typedef std::vector<int> eListType;
    double cutoff_radius;
    eListType elecs_inside;
    eListType::iterator current;
    IonData() : cutoff_radius(0.0), current(0) { }
  };


  /** @ingroup OrbitalComponent
   *  @brief Specialization for three-body Jastrow function using multiple functors
   *
   *Each pair-type can have distinct function \f$u(r_{ij})\f$.
   *For electrons, distinct pair correlation functions are used 
   *for spins up-up/down-down and up-down/down-up.
   */ 
  template<class FT>
  class eeI_JastrowOrbital: public OrbitalBase {

    const DistanceTableData* ee_table;
    const DistanceTableData* eI_table;

    //flag to prevent parallel output
    bool Write_Chiesa_Correction;
    //nuber of particles
    int Nelec, Nion;
    //N*N
    int NN;
    //number of groups of the target particleset
    int eGroups, iGroups;
    RealType DiffVal, DiffValSum;
    ParticleAttrib<RealType> U,d2U,curLap,curVal;
    ParticleAttrib<PosType> dU,curGrad;
    ParticleAttrib<PosType> curGrad0;
    RealType *FirstAddressOfdU, *LastAddressOfdU;
    // The first index is the ion, the second two are
    // the electrons
    Array<int,3> TripletID;

    std::map<std::string,FT*> J3Unique;
    ParticleSet *eRef, *IRef;
    bool FirstTime;
    RealType KEcorr;

    std::vector<IonData> IonDataList;

  public:

    typedef FT FuncType;

    ///container for the Jastrow functions 
    Array<FT*,3> F;

    eeI_JastrowOrbital(ParticleSet& ions, ParticleSet& elecs, bool is_master) 
      : Write_Chiesa_Correction(is_master), KEcorr(0.0)
      {
        eRef = &elecs;
	IRef = &ions;
        ee_table=DistanceTable::add(elecs);
        eI_table=DistanceTable::add(ions, elecs);
        init(elecs);
        FirstTime = true;
      }

    ~eeI_JastrowOrbital(){ }

    void init(ParticleSet& p) 
    {
      Nelec=p.getTotalNum();
      NN = Nelec*Nelec;
      Nion = IRef->getTotalNum();
      U.resize(Nelec*Nelec+1);
      d2U.resize(Nelec*Nelec);
      dU.resize(Nelec*Nelec);
      curGrad.resize(Nelec);
      curGrad0.resize(Nelec);
      curLap.resize(Nelec);
      curVal.resize(Nelec);

      FirstAddressOfdU = &(dU[0][0]);
      LastAddressOfdU = FirstAddressOfdU + dU.size()*DIM;

      TripletID.resize(Nion,Nelec,Nelec);
      int nisp=iGroups=IRef->getSpeciesSet().getTotalNum();
      int nesp=eGroups=p.groups();
      cerr << "nisp = " << nisp << endl;
      cerr << "nesp = " << nesp << endl;
      for (int i=0; i<Nion; i++)
	for(int j=0; j<Nelec; j++) 
	  for(int k=0; k<Nelec; k++)
	    TripletID(i,j,k) = IRef->GroupID[i]*nesp*nesp +
	      p.GroupID[j]*nesp + p.GroupID[k];
      F.resize(nisp,nesp,nesp);
      F = 0;

      IonDataList.resize(Nion);
    }

    void addFunc(const string& aname, int iSpecies, 
		 int eSpecies1, int eSpecies2, FT* j)
    {
      cerr << "iSpecies = " << iSpecies << "   eSpecies1 = " << eSpecies1
	   << "   eSpecies2 = " << eSpecies2 << endl;
      cerr << "eGroups = " << eGroups << endl;

      if(eSpecies1==eSpecies2)
      {
        if(eSpecies1==0) { //first time, assign everything
          int ijk = iSpecies * eGroups*eGroups;
	  for (int eG1=0; eG1<eGroups; eG1++)
	    for (int eG2=0; eG2<eGroups; eG2++, ijk++)
	      if(F(iSpecies, eG1, eG2)==0) 
		F(iSpecies, eG1, eG2) = j;
        }
      }
      else {
	F(iSpecies,eSpecies1,eSpecies2) = j;
	//if (eSpecies1 < eSpecies2)
	  F(iSpecies, eSpecies2, eSpecies1) = j;
        // F[ia*NumGroups+ib]=j;
        // if(ia<ib) F[ib*NumGroups+ia]=j; 
      }
      
      // Make sure cutoff radii are the same for all functors with the same
      // iSpecies
      double rcut = 0.5 * F(iSpecies,0,0)->cutoff_radius;
      cerr << "rcut = " << rcut << endl;
      for (int i=0; i<Nion; i++) 
	if (IRef->GroupID[i] == iSpecies)
	  IonDataList[i].cutoff_radius = rcut;

      for (int eG1=0; eG1<eGroups; eG1++)
	for (int eG2=0; eG2<eGroups; eG2++)
	  if (0.5*F(iSpecies,eG1,eG2)->cutoff_radius != rcut) {
	    app_error() << "eeI functors for ion species " << iSpecies
			<< " have different radii.  Aborting.\n";
	    abort();
	  }
      
      J3Unique[aname]=j;

      //      ChiesaKEcorrection();
      FirstTime = false;
    }

    //evaluate the distance table with els
    void resetTargetParticleSet(ParticleSet& P) 
    {
      ee_table = DistanceTable::add(P);
      eRef = &P;
      //      if(dPsi) dPsi->resetTargetParticleSet(P);
    }

    /** check in an optimizable parameter
     * @param o a super set of optimizable variables
     */
    void checkInVariables(opt_variables_type& active)
    {
      myVars.clear();
      typename std::map<std::string,FT*>::iterator it(J3Unique.begin()),it_end(J3Unique.end());
      while(it != it_end) 
      {
        (*it).second->checkInVariables(active);
        (*it).second->checkInVariables(myVars);
        ++it;
      }
      reportStatus(cout);
      
    }

    /** check out optimizable variables
     */
    void checkOutVariables(const opt_variables_type& active)
    {
      myVars.getIndex(active);
      Optimizable=myVars.is_optimizable();
      typename std::map<std::string,FT*>::iterator it(J3Unique.begin()),it_end(J3Unique.end());
      while(it != it_end) 
      {
        (*it++).second->checkOutVariables(active);
      }
      //      if(dPsi) dPsi->checkOutVariables(active);
    }

    ///reset the value of all the unique Two-Body Jastrow functions
    void resetParameters(const opt_variables_type& active)
    {
      if(!Optimizable) return;
      typename std::map<std::string,FT*>::iterator it(J3Unique.begin()),it_end(J3Unique.end());
      while(it != it_end) 
      {
        (*it++).second->resetParameters(active); 
      }

      //if (FirstTime) {
      if(!IsOptimizing)
      {
        app_log() << "  Chiesa kinetic energy correction = " 
          << ChiesaKEcorrection() << endl;
        //FirstTime = false;
      }

      //      if(dPsi) dPsi->resetParameters( active );
      for(int i=0; i<myVars.size(); ++i)
      {
        int ii=myVars.Index[i];
        if(ii>=0) myVars[i]= active[ii];
      }
    }

    /** print the state, e.g., optimizables */
    void reportStatus(ostream& os)
    {
      typename std::map<std::string,FT*>::iterator it(J3Unique.begin()),it_end(J3Unique.end());
      while(it != it_end) 
      {
        (*it).second->myVars.print(os);
        ++it;
      }
      ChiesaKEcorrection();
    }


    /** 
     *@param P input configuration containing N particles
     *@param G a vector containing N gradients
     *@param L a vector containing N laplacians
     *@param G returns the gradient \f$G[i]={\bf \nabla}_i J({\bf R})\f$
     *@param L returns the laplacian \f$L[i]=\nabla^2_i J({\bf R})\f$
     *@return \f$exp(-J({\bf R}))\f$
     *@brief While evaluating the value of the Jastrow for a set of
     *particles add the gradient and laplacian contribution of the
     *Jastrow to G(radient) and L(aplacian) for local energy calculations
     *such that \f[ G[i]+={\bf \nabla}_i J({\bf R}) \f] 
     *and \f[ L[i]+=\nabla^2_i J({\bf R}). \f]
     *@note The DistanceTableData contains only distinct pairs of the 
     *particles belonging to one set, e.g., SymmetricDTD.
     */
    RealType evaluateLog(ParticleSet& P,
		          ParticleSet::ParticleGradient_t& G, 
		          ParticleSet::ParticleLaplacian_t& L) {

      LogValue=0.0;

      // First, create lists of electrons within the sphere of each ion
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	ion.elecs_inside.clear();
	int iel=0;
	if (ion.cutoff_radius > 0.0) 
	  for (int nn=eI_table->M[i]; nn<eI_table->M[i+1]; nn++, iel++) 
	    if (eI_table->r(nn) < ion.cutoff_radius)
	      ion.elecs_inside.push_back(iel);
      }

      RealType u;
      PosType gradF;
      Tensor<RealType,3> hessF;
      // Now, evaluate three-body term for each ion
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	int nn0 = eI_table->M[i];
	for (int j=0; j<ion.elecs_inside.size(); j++) {
	  int jel = ion.elecs_inside[j];
	  // cerr << "jel = " << jel << " dtable j = " << eI_table->J[nn0+jel] << endl;
	  RealType r_Ij     = eI_table->r(nn0+jel);
	  RealType r_Ij_inv = eI_table->rinv(nn0+jel);
	  int ee0 = ee_table->M[jel]-(jel+1);
	  for (int k=j+1; k<ion.elecs_inside.size(); k++) {
	    int kel = ion.elecs_inside[k];
	    // cerr << "kel = " << kel << " dtable k = " << ee_table->J[ee0+kel] << endl;
	    // cerr << "jel,kel = " << jel << ", " << kel << endl;
	    RealType r_Ik     = eI_table->r(nn0+kel);
	    RealType r_Ik_inv = eI_table->rinv(nn0+kel);
	    RealType r_jk     = ee_table->r(ee0+kel);
	    RealType r_jk_inv = ee_table->rinv(ee0+kel);
	    FT &func = *F.data()[TripletID(i, jel, kel)];
	    u = func.evaluate (r_jk, r_Ij, r_Ik, gradF, hessF);
	    LogValue -= u;

	    PosType gr_ee =    gradF[0]*r_jk_inv * ee_table->dr(ee0+kel);
	    PosType du_j, du_k;
	    RealType d2u_j, d2u_k;
	    du_j = gradF[1]*r_Ij_inv * eI_table->dr(nn0+jel) - gr_ee;
	    du_k = gradF[2]*r_Ik_inv * eI_table->dr(nn0+kel) + gr_ee;
	    d2u_j = (hessF(0,0) + 2.0*r_jk_inv*gradF[0] -
		     2.0*hessF(0,1)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+jel))*r_jk_inv*r_Ij_inv
		     + hessF(1,1) + 2.0*r_Ij_inv*gradF[1]);
	    d2u_k = (hessF(0,0) + 2.0*r_jk_inv*gradF[0] +
		     2.0*hessF(0,2)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+kel))*r_jk_inv*r_Ik_inv
		     + hessF(2,2) + 2.0*r_Ik_inv*gradF[2]);
	    G[jel] -= du_j;
	    G[kel] -= du_k;
	    L[jel] -= d2u_j;
	    L[kel] -= d2u_k;

	    // PosType gr_ee =    gradF[0]*r_jk_inv * ee_table->dr(ee0+kel);
	    // G[jel] +=  gr_ee - gradF[1]*r_Ij_inv * eI_table->dr(nn0+jel);
	    // G[kel] -=  gr_ee + gradF[2]*r_Ik_inv * eI_table->dr(nn0+kel);
	    // L[jel] -= (hessF(0,0) + 2.0*r_jk_inv*gradF[0] -
	    // 	       2.0*hessF(0,1)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+jel))*r_jk_inv*r_Ij_inv
	    // 	       + hessF(1,1) + 2.0*r_Ij_inv*gradF[1]);
	    // L[kel] -= (hessF(0,0) + 2.0*r_jk_inv*gradF[0] +
	    // 	       2.0*hessF(0,2)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+kel))*r_jk_inv*r_Ik_inv
	    // 	       + hessF(2,2) + 2.0*r_Ik_inv*gradF[2]);
	  }
	}
      }
      return LogValue;
      // RealType dudr, d2udr2;
      // PosType gr;
      // for(int i=0; i<ee_table->size(SourceIndex); i++) {
      // 	for(int nn=ee_table->M[i]; nn<ee_table->M[i+1]; nn++) {
      // 	  int j = ee_table->J[nn];
      // 	  //LogValue -= F[ee_table->PairID[nn]]->evaluate(ee_table->r(nn), dudr, d2udr2);
      // 	  RealType uij = F[ee_table->PairID[nn]]->evaluate(ee_table->r(nn), dudr, d2udr2);
      //     LogValue -= uij;
      // 	  U[i*Nelec+j]=uij; U[j*Nelec+i]=uij; //save for the ratio
      // 	  //multiply 1/r
      // 	  dudr *= ee_table->rinv(nn);
      // 	  gr = dudr*ee_table->dr(nn);
      // 	  //(d^2 u \over dr^2) + (2.0\over r)(du\over\dr)
      // 	  RealType lap(d2udr2+2.0*dudr);

      // 	  //multiply -1
      // 	  G[i] += gr;
      // 	  G[j] -= gr;
      // 	  L[i] -= lap; 
      // 	  L[j] -= lap; 
      // 	}
      // }
    }

    ValueType evaluate(ParticleSet& P,
		       ParticleSet::ParticleGradient_t& G, 
		       ParticleSet::ParticleLaplacian_t& L) 
    {
      return std::exp(evaluateLog(P,G,L));
    }

    ValueType ratio(ParticleSet& P, int iat) 
    {
      cerr << "Called ratio.\n";
      curVal=0.0;

      int ee0 = ee_table->M[iat] - (iat+1);

      RealType diff = 0.0;
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	RealType r_Ii = eI_table->Temp[iat].r1;
	int nn0 = eI_table->M[i];
	if (r_Ii < ion.cutoff_radius) {
	  for (int j=0; j<ion.elecs_inside.size(); j++) {
	    int jat = ion.elecs_inside[j];
	    if (jat != iat) {
	      RealType r_ij = ee_table->Temp[jat].r1;
	      RealType r_Ij = eI_table->r(nn0+jat);
	      FT &func = *F.data()[TripletID(i, iat, jat)];
	      RealType u = func.evaluate(r_ij, r_Ii, r_Ij);
	      curVal[jat] += u;
	      diff += u; 
	    }
	  }
	}
	//if (Fs[i]) curVal += Fs[i]->evaluate(d_table->Temp[i].r1);
      }
      for (int jat=0; jat<Nelec; jat++)
	diff -= U[iat*Nelec+jat];
      return std::exp(diff);
      //return std::exp(U[iat]-curVal);
      // DiffVal=0.0;
      // const int* pairid(PairID[iat]);
      // for(int jat=0, ij=iat*N; jat<N; jat++,ij++) {
      //   if(jat == iat) {
      //     curVal[jat]=0.0;
      //   } else {
      //     curVal[jat]=F[pairid[jat]]->evaluate(ee_table->Temp[jat].r1);
      //     DiffVal += U[ij]-curVal[jat];
      //     //DiffVal += U[ij]-F[pairid[jat]]->evaluate(ee_table->Temp[jat].r1);
      //   }
      // }
      // return std::exp(DiffVal);
    }

    /** later merge the loop */
    ValueType ratio(ParticleSet& P, int iat,
		    ParticleSet::ParticleGradient_t& dG,
		    ParticleSet::ParticleLaplacian_t& dL)  
    {
      cerr << "ratio(P,iat,dG,dL) called.\n";
      curVal=0.0;
      curGrad = PosType();

      int ee0 = ee_table->M[iat] - (iat+1);

      RealType diff = 0.0;
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	RealType r_Ii     = eI_table->Temp[iat].r1;	
	RealType r_Ii_inv = 1.0/r_Ii;
	int nn0 = eI_table->M[i];
	if (r_Ii < ion.cutoff_radius) {
	  for (int j=0; j<ion.elecs_inside.size(); j++) {
	    int jat = ion.elecs_inside[j];
	    if (jat != iat) {
	      RealType r_ij = ee_table->Temp[jat].r1;
	      RealType r_ij_inv = 1.0/r_ij;
	      RealType r_Ij = eI_table->r(nn0+jat);
	      RealType r_Ij_inv = 1.0/r_Ij;
	      FT &func = *F.data()[TripletID(i, iat, jat)];

	      PosType gradF;
	      Tensor<RealType,OHMMS_DIM> hessF;
	      RealType u = func.evaluate(r_ij, r_Ii, r_Ij, gradF, hessF);
	      PosType gr_ee =    gradF[0]*r_ij_inv * ee_table->Temp[jat].dr1;
	      PosType du_i, du_j;
	      RealType d2u_i, d2u_j;
	      du_i = gradF[1]*r_Ii_inv * eI_table->Temp[iat].dr1 - gr_ee;
	      du_j = gradF[2]*r_Ij_inv * eI_table->dr(nn0+jat) + gr_ee;

	      curVal [jat] += u;
	      curGrad[jat] += du_j;
	      // dG += ?
	      diff += u;
	    }
	  }
	}
      }
      for (int jat=0; jat<Nelec; jat++) 
	diff -= U[iat*Nelec+jat];

      cerr << "diff = " << diff << endl;
      return std::exp(diff);

      // register RealType dudr, d2udr2,u;
      // register PosType gr;
      // DiffVal = 0.0;      
      // const int* pairid = PairID[iat];
      // for(int jat=0, ij=iat*Nelec; jat<Nelec; jat++,ij++) 
      // {
      // 	if(jat==iat) {
      // 	  curVal[jat] = 0.0;curGrad[jat]=0.0; curLap[jat]=0.0;
      // 	} else {
      // 	  curVal[jat] = F[pairid[jat]]->evaluate(ee_table->Temp[jat].r1, dudr, d2udr2);
      // 	  dudr *= ee_table->Temp[jat].rinv1;
      // 	  curGrad[jat] = -dudr*ee_table->Temp[jat].dr1;
      // 	  curLap[jat] = -(d2udr2+2.0*dudr);
      // 	  DiffVal += (U[ij]-curVal[jat]);
      // 	}
      // }
      // PosType sumg,dg;
      // RealType suml=0.0,dl;
      // for(int jat=0,ij=iat*Nelec,ji=iat; jat<Nelec; jat++,ij++,ji+=Nelec) {
      // 	sumg += (dg=curGrad[jat]-dU[ij]);
      // 	suml += (dl=curLap[jat]-d2U[ij]);
      //   dG[jat] -= dg;
      // 	dL[jat] += dl;
      // }
      // dG[iat] += sumg;
      // dL[iat] += suml;     

      // curGrad0=curGrad;

      // return std::exp(DiffVal);
    }

    GradType evalGrad(ParticleSet& P, int iat)
    {
      GradType gr;
      //for(int jat=0,ij=iat*Nelec; jat<Nelec; ++jat,++ij) gr += dU[ij];
      return gr;
    }

    ValueType ratioGrad(ParticleSet& P, int iat, GradType& grad_iat)
    {
      
      return 1.0;
      // RealType dudr, d2udr2,u;
      // PosType gr;
      // const int* pairid = PairID[iat];
      // DiffVal = 0.0;      
      // for(int jat=0, ij=iat*Nelec; jat<Nelec; jat++,ij++) 
      // {
      // 	if(jat==iat) 
      //   {
      // 	  curVal[jat] = 0.0;curGrad[jat]=0.0; curLap[jat]=0.0;
      // 	} 
      //   else 
      //   {
      // 	  curVal[jat] = F[pairid[jat]]->evaluate(ee_table->Temp[jat].r1, dudr, d2udr2);
      // 	  dudr *= ee_table->Temp[jat].rinv1;
      // 	  gr += curGrad[jat] = -dudr*ee_table->Temp[jat].dr1;
      // 	  curLap[jat] = -(d2udr2+2.0*dudr);
      // 	  DiffVal += (U[ij]-curVal[jat]);
      // 	}
      // }
      // grad_iat += gr;
      // //curGrad0-=curGrad;
      // //cout << "RATIOGRAD " << curGrad0 << endl;

      // return std::exp(DiffVal);
    }

    ///** later merge the loop */
    //ValueType logRatio(ParticleSet& P, int iat,
    //    	    ParticleSet::ParticleGradient_t& dG,
    //    	    ParticleSet::ParticleLaplacian_t& dL)  {
    //  register RealType dudr, d2udr2,u;
    //  register PosType gr;
    //  DiffVal = 0.0;      
    //  const int* pairid = PairID[iat];
    //  for(int jat=0, ij=iat*Nelec; jat<Nelec; jat++,ij++) {
    //    if(jat==iat) {
    //      curVal[jat] = 0.0;curGrad[jat]=0.0; curLap[jat]=0.0;
    //    } else {
    //      curVal[jat] = F[pairid[jat]]->evaluate(ee_table->Temp[jat].r1, dudr, d2udr2);
    //      dudr *= ee_table->Temp[jat].rinv1;
    //      curGrad[jat] = -dudr*ee_table->Temp[jat].dr1;
    //      curLap[jat] = -(d2udr2+2.0*dudr);
    //      DiffVal += (U[ij]-curVal[jat]);
    //    }
    //  }
    //  PosType sumg,dg;
    //  RealType suml=0.0,dl;
    //  for(int jat=0,ij=iat*Nelec,ji=iat; jat<Nelec; jat++,ij++,ji+=Nelec) {
    //    sumg += (dg=curGrad[jat]-dU[ij]);
    //    suml += (dl=curLap[jat]-d2U[ij]);
    //    dG[jat] -= dg;
    //    dL[jat] += dl;
    //  }
    //  dG[iat] += sumg;
    //  dL[iat] += suml;     
    //  return DiffVal;
    //}

    inline void restore(int iat) {}

    void acceptMove(ParticleSet& P, int iat) { 
      DiffValSum += DiffVal;
      for(int jat=0,ij=iat*Nelec,ji=iat; jat<Nelec; jat++,ij++,ji+=Nelec) {
	dU[ij]=curGrad[jat]; 
	//dU[ji]=-1.0*curGrad[jat];
	dU[ji]=curGrad[jat]*-1.0;
	d2U[ij]=d2U[ji] = curLap[jat];
	U[ij] =  U[ji] = curVal[jat];
      }
    }


    inline void update(ParticleSet& P, 		
		       ParticleSet::ParticleGradient_t& dG, 
		       ParticleSet::ParticleLaplacian_t& dL,
		       int iat) {
      DiffValSum += DiffVal;
      GradType sumg,dg;
      ValueType suml=0.0,dl;
      for(int jat=0,ij=iat*Nelec,ji=iat; jat<Nelec; jat++,ij++,ji+=Nelec) {
	sumg += (dg=curGrad[jat]-dU[ij]);
	suml += (dl=curLap[jat]-d2U[ij]);
	dU[ij]=curGrad[jat]; 
	//dU[ji]=-1.0*curGrad[jat];
	dU[ji]=curGrad[jat]*-1.0;
	d2U[ij]=d2U[ji] = curLap[jat];
	U[ij] =  U[ji] = curVal[jat];
        dG[jat] -= dg;
	dL[jat] += dl;
      }
      dG[iat] += sumg;
      dL[iat] += suml;     
    }


    inline void evaluateLogAndStore(ParticleSet& P, 
		       ParticleSet::ParticleGradient_t& G, 
		       ParticleSet::ParticleLaplacian_t& L) 
    {
      cerr << "evaluateLogAndStore called.\n";
      LogValue=0.0;

      // First, create lists of electrons within the sphere of each ion
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	ion.elecs_inside.clear();
	int iel=0;
	if (ion.cutoff_radius > 0.0) 
	  for (int nn=eI_table->M[i]; nn<eI_table->M[i+1]; nn++, iel++) 
	    if (eI_table->r(nn) < ion.cutoff_radius)
	      ion.elecs_inside.push_back(iel);
      }

      RealType u;
      PosType gradF;
      Tensor<RealType,3> hessF;

      // Zero out cached data
      for (int jk=0; jk<Nelec*Nelec; jk++) {
	U[jk] = 0.0;
	dU[jk] = PosType();
	d2U[jk] = 0.0;
      }

      // Now, evaluate three-body term for each ion
      for (int i=0; i<Nion; i++) {
	IonData &ion = IonDataList[i];
	int nn0 = eI_table->M[i];
	for (int j=0; j<ion.elecs_inside.size(); j++) {
	  int jel = ion.elecs_inside[j];
	  RealType r_Ij     = eI_table->r(nn0+jel);
	  RealType r_Ij_inv = eI_table->rinv(nn0+jel);
	  int ee0 = ee_table->M[jel]-(jel+1);
	  for (int k=j+1; k<ion.elecs_inside.size(); k++) {
	    int kel = ion.elecs_inside[k];
	    RealType r_Ik     = eI_table->r(nn0+kel);
	    RealType r_Ik_inv = eI_table->rinv(nn0+kel);
	    RealType r_jk     = ee_table->r(ee0+kel);
	    RealType r_jk_inv = ee_table->rinv(ee0+kel);
	    FT &func = *F.data()[TripletID(i, jel, kel)];

	    u = func.evaluate (r_jk, r_Ij, r_Ik, gradF, hessF);
	    LogValue -= u;
	    PosType gr_ee =    gradF[0]*r_jk_inv * ee_table->dr(ee0+kel);
	    PosType du_j, du_k;
	    RealType d2u_j, d2u_k;
	    du_j = gradF[1]*r_Ij_inv * eI_table->dr(nn0+jel) - gr_ee;
	    du_k = gradF[2]*r_Ik_inv * eI_table->dr(nn0+kel) + gr_ee;
	    d2u_j = (hessF(0,0) + 2.0*r_jk_inv*gradF[0] -
		     2.0*hessF(0,1)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+jel))*r_jk_inv*r_Ij_inv
		     + hessF(1,1) + 2.0*r_Ij_inv*gradF[1]);
	    d2u_k = (hessF(0,0) + 2.0*r_jk_inv*gradF[0] +
		     2.0*hessF(0,2)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+kel))*r_jk_inv*r_Ik_inv
		     + hessF(2,2) + 2.0*r_Ik_inv*gradF[2]);
	    G[jel] -= du_j;
	    G[kel] -= du_k;
	    L[jel] -= d2u_j;
	    L[kel] -= d2u_k;

	    int jk = jel*Nelec+kel; 
	    int kj = kel*Nelec+jel;
	    U[jk] += u;
	    U[kj] += u;
	    dU[jk] += du_j;
	    dU[kj] += du_k;

	    // G[jel] +=  gr_ee - gradF[1]*r_Ij_inv * eI_table->dr(nn0+jel);
	    // G[kel] -=  gr_ee + gradF[2]*r_Ik_inv * eI_table->dr(nn0+kel);
	    // L[jel] -= (hessF(0,0) + 2.0*r_jk_inv*gradF[0] -
	    // 	       2.0*hessF(0,1)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+jel))*r_jk_inv*r_Ij_inv
	    // 	       + hessF(1,1) + 2.0*r_Ij_inv*gradF[1]);
	    // L[kel] -= (hessF(0,0) + 2.0*r_jk_inv*gradF[0] +
	    // 	       2.0*hessF(0,2)*dot(ee_table->dr(ee0+kel),eI_table->dr(nn0+kel))*r_jk_inv*r_Ik_inv
	    // 	       + hessF(2,2) + 2.0*r_Ik_inv*gradF[2]);
	    
	  }
	}
      }

      // if (FirstTime) {
      // 	FirstTime = false;
      // 	ChiesaKEcorrection();
      // }

      // RealType dudr, d2udr2,u;
      // LogValue=0.0;
      // GradType gr;
      // for(int i=0; i<ee_table->size(SourceIndex); i++) {
      // 	for(int nn=ee_table->M[i]; nn<ee_table->M[i+1]; nn++) {
      // 	  int j = ee_table->J[nn];
      // 	  u = F[ee_table->PairID[nn]]->evaluate(ee_table->r(nn), dudr, d2udr2);
      // 	  LogValue -= u;
      // 	  dudr *= ee_table->rinv(nn);
      // 	  gr = dudr*ee_table->dr(nn);
      // 	  //(d^2 u \over dr^2) + (2.0\over r)(du\over\dr)\f$
      // 	  RealType lap = d2udr2+2.0*dudr;
      // 	  int ij = i*Nelec+j, ji=j*Nelec+i;
      // 	  U[ij]=u; U[ji]=u;
      // 	  //dU[ij] = gr; dU[ji] = -1.0*gr;
      // 	  dU[ij] = gr; dU[ji] = gr*-1.0;
      // 	  d2U[ij] = -lap; d2U[ji] = -lap;

      // 	  //add gradient and laplacian contribution
      // 	  dG[i] += gr;
      // 	  dG[j] -= gr;
      // 	  dL[i] -= lap; 
      // 	  dL[j] -= lap; 
      // 	}
      // }
    }

    inline RealType registerData(ParticleSet& P, PooledData<RealType>& buf){
      cerr << "Called registerData.\n";
      // cerr<<"REGISTERING 2 BODY JASTROW"<<endl;
      evaluateLogAndStore(P,P.G,P.L);
      //LogValue=0.0;
      //RealType dudr, d2udr2,u;
      //GradType gr;
      //PairID.resize(ee_table->size(SourceIndex),ee_table->size(SourceIndex));
      //int nsp=P.groups();
      //for(int i=0; i<ee_table->size(SourceIndex); i++)
      //  for(int j=0; j<ee_table->size(SourceIndex); j++) 
      //    PairID(i,j) = P.GroupID[i]*nsp+P.GroupID[j];

      //for(int i=0; i<ee_table->size(SourceIndex); i++) {
      //  for(int nn=ee_table->M[i]; nn<ee_table->M[i+1]; nn++) {
      //    int j = ee_table->J[nn];
      //    //ValueType sumu = F.evaluate(ee_table->r(nn));
      //    u = F[ee_table->PairID[nn]]->evaluate(ee_table->r(nn), dudr, d2udr2);
      //    LogValue -= u;
      //    dudr *= ee_table->rinv(nn);
      //    gr = dudr*ee_table->dr(nn);
      //    //(d^2 u \over dr^2) + (2.0\over r)(du\over\dr)\f$
      //    RealType lap = d2udr2+2.0*dudr;
      //    int ij = i*Nelec+j, ji=j*Nelec+i;
      //    U[ij]=u; U[ji]=u;
      //    //dU[ij] = gr; dU[ji] = -1.0*gr;
      //    dU[ij] = gr; dU[ji] = gr*-1.0;
      //    d2U[ij] = -lap; d2U[ji] = -lap;

      //    //add gradient and laplacian contribution
      //    P.G[i] += gr;
      //    P.G[j] -= gr;
      //    P.L[i] -= lap; 
      //    P.L[j] -= lap; 
      //  }
      //}

      U[NN]= real(LogValue);
      buf.add(U.begin(), U.end());
      buf.add(d2U.begin(), d2U.end());
      buf.add(FirstAddressOfdU,LastAddressOfdU);

      return LogValue;
    }

    inline RealType updateBuffer(ParticleSet& P, PooledData<RealType>& buf,
        bool fromscratch=false){
      evaluateLogAndStore(P,P.G,P.L);
      //RealType dudr, d2udr2,u;
      //LogValue=0.0;
      //GradType gr;
      //PairID.resize(ee_table->size(SourceIndex),ee_table->size(SourceIndex));
      //int nsp=P.groups();
      //for(int i=0; i<ee_table->size(SourceIndex); i++)
      //  for(int j=0; j<ee_table->size(SourceIndex); j++) 
      //    PairID(i,j) = P.GroupID[i]*nsp+P.GroupID[j];

      //for(int i=0; i<ee_table->size(SourceIndex); i++) {
      //  for(int nn=ee_table->M[i]; nn<ee_table->M[i+1]; nn++) {
      //    int j = ee_table->J[nn];
      //    //ValueType sumu = F.evaluate(ee_table->r(nn));
      //    u = F[ee_table->PairID[nn]]->evaluate(ee_table->r(nn), dudr, d2udr2);
      //    LogValue -= u;
      //    dudr *= ee_table->rinv(nn);
      //    gr = dudr*ee_table->dr(nn);
      //    //(d^2 u \over dr^2) + (2.0\over r)(du\over\dr)\f$
      //    RealType lap = d2udr2+2.0*dudr;
      //    int ij = i*N+j, ji=j*N+i;
      //    U[ij]=u; U[ji]=u;
      //    //dU[ij] = gr; dU[ji] = -1.0*gr;
      //    dU[ij] = gr; dU[ji] = gr*-1.0;
      //    d2U[ij] = -lap; d2U[ji] = -lap;

      //    //add gradient and laplacian contribution
      //    P.G[i] += gr;
      //    P.G[j] -= gr;
      //    P.L[i] -= lap; 
      //    P.L[j] -= lap; 
      //  }
      //}

      U[NN]= real(LogValue);
      buf.put(U.begin(), U.end());
      buf.put(d2U.begin(), d2U.end());
      buf.put(FirstAddressOfdU,LastAddressOfdU);
      return LogValue;
    }
    
    inline void copyFromBuffer(ParticleSet& P, PooledData<RealType>& buf) {
      buf.get(U.begin(), U.end());
      buf.get(d2U.begin(), d2U.end());
      buf.get(FirstAddressOfdU,LastAddressOfdU);
      DiffValSum=0.0;
    }

    inline RealType evaluateLog(ParticleSet& P, PooledData<RealType>& buf) {
      cerr << "Called evaluateLog (P, buf).\n";
      RealType x = (U[NN] += DiffValSum);
      buf.put(U.begin(), U.end());
      buf.put(d2U.begin(), d2U.end());
      buf.put(FirstAddressOfdU,LastAddressOfdU);
      return x;
    }

    OrbitalBasePtr makeClone(ParticleSet& tqp) const
    {
      eeI_JastrowOrbital<FT>* j2copy=new eeI_JastrowOrbital<FT>(*IRef, tqp,false);
      //      if (dPsi) j2copy->dPsi = dPsi->makeClone(tqp);
      map<const FT*,FT*> fcmap;
      for (int iG=0; iG<iGroups; iG++)
	for (int eG1=0; eG1<eGroups; eG1++)
	  for (int eG2=0; eG2<eGroups; eG2++) {
	    int ijk = iG*eGroups*eGroups + eG1*eGroups + eG2;
	    if(F(iG,eG1,eG2)==0) continue;
	    typename map<const FT*,FT*>::iterator fit=fcmap.find(F(iG,eG1,eG2));
	    if(fit == fcmap.end()) {
	      FT* fc=new FT(*F(iG,eG1,eG2));
	      stringstream aname;
	      aname << iG << eG1 << eG2;
	      j2copy->addFunc(aname.str(),iG, eG1, eG2, fc);
	      //if (dPsi) (j2copy->dPsi)->addFunc(aname.str(),ig,jg,fc);
	      fcmap[F(iG,eG1,eG2)]=fc;
          }
        }
        
      j2copy->Optimizable = Optimizable;
        
      return j2copy;
    }

    void copyFrom(const OrbitalBase& old)
    {
      //nothing to do
    }

    RealType ChiesaKEcorrection()
    {
    }

    void
    finalizeOptimization()
    {
      ChiesaKEcorrection();
    }

    RealType KECorrection()
    {
      return KEcorr;
    }
    
  };




}
#endif
/***************************************************************************
 * $RCSfile$   $Author: kesler $
 * $Revision: 3708 $   $Date: 2009-03-25 17:30:09 -0500 (Wed, 25 Mar 2009) $
 * $Id: eeI_JastrowOrbital.h 3708 2009-03-25 22:30:09Z kesler $ 
 ***************************************************************************/

