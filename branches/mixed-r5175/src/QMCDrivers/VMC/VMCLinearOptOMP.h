//////////////////////////////////////////////////////////////////
// (c) Copyright 2003- by Jeongnim Kim
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
#ifndef QMCPLUSPLUS_VMCLINEAROPT_OMP_H
#define QMCPLUSPLUS_VMCLINEAROPT_OMP_H
#include "QMCDrivers/QMCDriver.h"
#include "QMCDrivers/CloneManager.h"
#include "Message/CommOperators.h"
namespace qmcplusplus
{

  /** @ingroup QMCDrivers  ParticleByParticle
   * @brief Implements a VMC using particle-by-particle move. Threaded execution.
   */
  class VMCLinearOptOMP: public QMCDriver, public CloneManager
  {
    public:
      /// Constructor.
      VMCLinearOptOMP(MCWalkerConfiguration& w, TrialWaveFunction& psi, QMCHamiltonian& h,
          HamiltonianPool& hpool, WaveFunctionPool& ppool);

      ~VMCLinearOptOMP()
      {
        if (UseDrift == "rn")
          delete_iter(CSMovers.begin(),CSMovers.end());
      }
      bool run();
      RealType runCS(vector<RealType>& curParams, vector<RealType>& curDir, vector<RealType>& lambdas);
      int runCS(vector<vector<RealType> >& bestParams, RealType& errorbars);
      bool put(xmlNodePtr cur);

      inline void getDeltaCosts(std::vector<RealType>& cstVec)
      {
        for(int i=0;i<NE_i.size();i++) cstVec[i]=NE_i[i];
      }

//       void fillMatrices(Matrix<RealType>& H2, Matrix<RealType>& Hamiltonian, Matrix<RealType>& Variance, Matrix<RealType>& Overlap);
      RealType fillOverlapHamiltonianMatrices(Matrix<RealType>& LeftM, Matrix<RealType>& RightM);

      inline void clearComponentMatrices()
      {
        
        Ham2=0.0;
        Ham=0.0;
        Olp=0.0;
        DerivRecords=0.0;
        HDerivRecords=0.0;

        for(int i=0;i<D_E.size();i++)
        {
          D_E[i]=0.0;
          HD[i]=0.0;
          HD2[i]=0.0;
          D[i]=0.0;
        }
        sE=0;
        sE2=0;
        sE4=0;
        sW=0;
        sN=0;
      }

    private:
      ///number of warmup steps
      int myWarmupSteps;
      ///number of RN warmup steps
      int myRNWarmupSteps;
      ///period for walker dump
      int myPeriod4WalkerDump;
      ///option to enable/disable drift equation for VMC
      string UseDrift;
      ///target errorbars to use to determine when to stop for filling matrix and line minimization
      RealType alpha_errorbars, beta_errorbars;
      ///check the run-time environments
      void resetRun();
      ///copy constructor
      VMCLinearOptOMP(const VMCLinearOptOMP& a): QMCDriver(a),CloneManager(a) { }
      /// Copy operator (disabled).
      VMCLinearOptOMP& operator=(const VMCLinearOptOMP&)
      {
        return *this;
      }
      ///Ways to set rn constant
      RealType logoffset,logepsilon;

      int NumOptimizables;
      RealType w_beta,w_alpha;
      RealType E_avg, V_avg;
      string GEVtype;
      vector<RandomGenerator_t> RngSaved;

      ///These are the values we collect to build the Matrices GLOBAL
      Matrix<RealType> Ham, Ham2, Olp;
      std::vector<RealType> D_E, HD2, HD, D;
      RealType sE,sE2,sE4,sW,sN;
      ///Temp matrices
      Matrix<RealType> DerivRecords, HDerivRecords;


      void initCS();

      void resizeForOpt(int n)
      {
        Ham2.resize(n,n);
        Ham.resize(n,n);
        Olp.resize(n,n);
        DerivRecords.resize(NumThreads,n);
        HDerivRecords.resize(NumThreads,n);

        D_E.resize(n);
        HD.resize(n);
        HD2.resize(n);
        D.resize(n);

        clearComponentMatrices();
      }

      void clearCSEstimators()
      {
        CorrelatedH.resize(NumThreads,NumThreads);
        gCorrelatedH.resize(NumThreads,NumThreads);
        Norm2s.resize(NumThreads+1,NumThreads+1);
        gNorm2s.resize(NumThreads+1,NumThreads+1);
        Norms.resize(NumThreads+1); 
        Energies.resize(NumThreads);
        gNorms.resize(NumThreads+1); 
        gEnergies.resize(NumThreads);
        NE_i.resize(NumThreads);
        gCorrelatedH=CorrelatedH=0;
        gNorm2s=Norm2s=0;
        for (int ip=0; ip<NumThreads+1; ++ip) gNorms[ip]=Norms[ip]=0;
        for (int ip=0; ip<NumThreads; ++ip) gEnergies[ip]=Energies[ip]=0;
        for (int ip=0; ip<NumThreads; ++ip) NE_i[ip]=0;
      }

      void setWalkersEqual(Walker_t& firstWalker)
      {
        for (int ip=0; ip<NumThreads; ++ip)
        {
          (*W[ip]).makeCopy(firstWalker);
        }

        for (int ip=0; ip<NumThreads; ++ip)
        {
          Walker_t& thisWalker(*W[ip]);
          wClones[ip]->loadWalker(thisWalker,true);

          Walker_t::Buffer_t tbuffer;
          RealType logpsi=psiClones[ip]->evaluateLog(*wClones[ip]);
          logpsi=psiClones[ip]->registerData(*wClones[ip],tbuffer);
          thisWalker.DataSet=tbuffer;
          thisWalker.Weight = 1.0;
          RealType ene = hClones[ip]->evaluate( *wClones[ip]);
          //         app_log()<<ene<<" "<<logpsi<<endl;
          thisWalker.resetProperty(logpsi,psiClones[ip]->getPhase(),ene);
          hClones[ip]->saveProperty(thisWalker.getPropertyBase());
          wClones[ip]->saveWalker(thisWalker);
        }
        logpsi2_0_0 = W[0]->getPropertyBase()[LOGPSI];
        myComm->bcast(logpsi2_0_0);
      }


      RealType fillComponentMatrices();
      Walker_t firstWalker;
      Matrix<RealType> CorrelatedH, Norm2s;
      vector<RealType> Norms;
      vector<RealType> Energies;
      vector<RealType> NE_i;

      // global quantities for mpi collection
      std::vector<RealType> gEnergies, gNorms;
      Matrix<RealType> gNorm2s, gCorrelatedH;

      int CSBlock;
      int minE,nE;
      RealType logpsi2_0_0;
      RealType estimateCS();

      bool moved_right;
      bool moved_left;
      bool bracketing(vector<RealType>& lambdas, RealType errorbars);
      //     weights for correlated sampling
      vector<RealType> w_i;
  };

}
#endif
/***************************************************************************
 * $RCSfile: VMCLinearOptOMP.h,v $   $Author: jnkim $
 * $Revision: 1.5 $   $Date: 2006/07/17 14:29:40 $
 * $Id: VMCLinearOptOMP.h,v 1.5 2006/07/17 14:29:40 jnkim Exp $
 ***************************************************************************/
