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
#include "QMCDrivers/VMC/VMCcuda.h"
#include "OhmmsApp/RandomNumberControl.h"
#include "Utilities/RandomGenerator.h"
#include "ParticleBase/RandomSeqGenerator.h"
#include "QMCDrivers/DriftOperators.h"

namespace qmcplusplus { 

  /// Constructor.
  VMCcuda::VMCcuda(MCWalkerConfiguration& w, TrialWaveFunction& psi, QMCHamiltonian& h):
    QMCDriver(w,psi,h), myWarmupSteps(0), UseDrift("yes")
  { 
    RootName = "vmc";
    QMCType ="VMCcuda";
    QMCDriverMode.set(QMC_UPDATE_MODE,1);
    QMCDriverMode.set(QMC_WARMUP,0);
    m_param.add(UseDrift,"useDrift","string"); 
    m_param.add(UseDrift,"usedrift","string");
    m_param.add(myWarmupSteps,"warmupSteps","int");
    m_param.add(nTargetSamples,"targetWalkers","int");
  }
  
  bool VMCcuda::run() { 
    if (UseDrift == "yes")
      return runWithDrift();

    resetRun();
    IndexType block = 0;
    IndexType nAcceptTot = 0;
    IndexType nRejectTot = 0;
    IndexType updatePeriod= (QMCDriverMode[QMC_UPDATE_MODE]) 
      ? Period4CheckProperties 
      : (nBlocks+1)*nSteps;
    
    int nat = W.getTotalNum();
    int nw  = W.getActiveWalkers();
    
    vector<RealType>  LocalEnergy(nw);
    vector<PosType>   delpos(nw);
    vector<PosType>   newpos(nw);
    vector<ValueType> ratios(nw);
    vector<GradType>  oldG(nw), newG(nw);
    vector<ValueType> oldL(nw), newL(nw);
    vector<Walker_t*> accepted(nw);
    Matrix<ValueType> lapl(nw, nat);
    Matrix<GradType>  grad(nw, nat);
    double Esum;

    do {
      IndexType step = 0;
      nAccept = nReject = 0;
      Esum = 0.0;
      clock_t block_start = clock();
      Estimators->startBlock(nSteps);
      do
      {
        ++step;++CurrentStep;
        for(int iat=0; iat<nat; ++iat)
        {
          //create a 3N-Dimensional Gaussian with variance=1
          makeGaussRandomWithEngine(delpos,Random);
          for(int iw=0; iw<nw; ++iw) {
	    PosType G = W[iw]->Grad[iat];
            newpos[iw]=W[iw]->R[iat] + m_sqrttau*delpos[iw];
	    ratios[iw] = 1.0;
	  }
	  W.proposeMove_GPU(newpos, iat);

#ifdef CUDA_DEBUG
	  vector<RealType> logPsi1(W.size(), 0.0);
	  Psi.evaluateLog(W, logPsi1);
#endif
          Psi.ratio(W,iat,ratios,newG, newL);
	  
          accepted.clear();
	  vector<bool> acc(nw, false);
          for(int iw=0; iw<nw; ++iw) {
            if(ratios[iw]*ratios[iw] > Random()) {
              accepted.push_back(W[iw]);
	      nAccept++;
	      W[iw]->R[iat] = newpos[iw];
	      acc[iw] = true;
	    }
	    else 
	      nReject++;
	  }
	  W.acceptMove_GPU(acc);
	  if (accepted.size())
	    Psi.update(accepted,iat);

	 
#ifdef CUDA_DEBUG
	  vector<RealType> logPsi2(W.WalkerList.size(), 0.0);
	  Psi.evaluateLog(W, logPsi2);
	  for (int iw=0; iw<nw; iw++) {
	    if (acc[iw])
	      cerr << "ratio[" << iw << "] = " << ratios[iw]
		   << "  exp(Log2-Log1) = " << std::exp(logPsi2[iw]-logPsi1[iw]) << endl;
	  }
#endif
	  
	}

	// host_vector<TinyVector<CUDA_PRECISION,OHMMS_DIM> > Rcheck;
	// Rcheck = W[3]->R_GPU;
	// int ir = 253;
	// fprintf (stderr, "Cuda:  %16.8f %16.8f %16.8f\n",
	// 	 Rcheck[ir][0], Rcheck[ir][1], Rcheck[ir][2]);
	
	// fprintf (stderr, "Host:  %16.8f %16.8f %16.8f\n",
	// 	 W[3]->R[ir][0], W[3]->R[ir][1], W[3]->R[ir][2]);

	//H.saveProperty (W);
	Psi.gradLapl(W, grad, lapl);
	H.evaluate (W, LocalEnergy);
	Estimators->accumulate(W);
      } while(step<nSteps);
      Psi.recompute(W);

      // vector<RealType> logPsi(W.WalkerList.size(), 0.0);
      // Psi.evaluateLog(W, logPsi);
      
      double accept_ratio = (double)nAccept/(double)(nAccept+nReject);
      Estimators->stopBlock(accept_ratio);

      nAcceptTot += nAccept;
      nRejectTot += nReject;
      ++block;
      
      recordBlock(block);

      clock_t block_end = clock();
      double block_time = (double)(block_end-block_start)/CLOCKS_PER_SEC;
      // fprintf (stderr, "Block energy = %10.5f    "
      // 	       "Block accept ratio = %5.3f  Block time = %8.3f\n",
      // 	       Esum/(double)nSteps, accept_ratio, block_time);
      
    } while(block<nBlocks);

    //Mover->stopRun();

    //finalize a qmc section
    return finalize(block);
  }



  bool VMCcuda::runWithDrift() 
  { 
    resetRun();
    IndexType block = 0;
    IndexType nAcceptTot = 0;
    IndexType nRejectTot = 0;
    int nat = W.getTotalNum();
    int nw  = W.getActiveWalkers();
    
    vector<RealType>  LocalEnergy(nw), oldScale(nw), newScale(nw);
    vector<PosType>   delpos(nw);
    vector<PosType>   dr(nw);
    vector<PosType>   newpos(nw);
    vector<ValueType> ratios(nw), rplus(nw), rminus(nw);
    vector<PosType>  oldG(nw), newG(nw);
    vector<ValueType> oldL(nw), newL(nw);
    vector<Walker_t*> accepted(nw);
    Matrix<ValueType> lapl(nw, nat);
    Matrix<GradType>  grad(nw, nat);

    do {
      IndexType step = 0;
      nAccept = nReject = 0;
      clock_t block_start = clock();
      Estimators->startBlock(nSteps);
      do {
        step++;
	CurrentStep++;
        for(int iat=0; iat<nat; ++iat) {
	  Psi.getGradient (W, iat, oldG);
	  // for (int iw=0; iw<nw; iw++) 
	  //   newpos[iw] = W[iw]->R[iat] + 1.0e-3*PosType(0.0,1.0,0.0);
	  // W.proposeMove_GPU(newpos, iat);
	  // Psi.ratio(W,iat,rplus,newG, oldL);
	  // for (int iw=0; iw<nw; iw++) 
	  //   newpos[iw] = W[iw]->R[iat] - 1.0e-3*PosType(0.0,1.0,0.0);
	  // W.proposeMove_GPU(newpos, iat);
	  // Psi.ratio(W,iat,rminus,newG, oldL);
	  // if (iat == 12) {
	  //   fprintf (stderr, "FD  = %1.8e\n", (rplus[127] - rminus[127])/(2.0e-3));
	  //   fprintf (stderr, "old = %1.8e\n", oldG[127][1]);
	  //   fprintf (stderr, "new = %1.8e\n", newG[127][1]);
	  // }

          //create a 3N-Dimensional Gaussian with variance=1
          makeGaussRandomWithEngine(delpos,Random);
          for(int iw=0; iw<nw; iw++) {
	    oldScale[iw] = getDriftScale(m_tauovermass,oldG[iw]);
	    oldScale[iw] = 0.5*Tau;
	    dr[iw] = m_sqrttau*delpos[iw] + oldScale[iw]*oldG[iw];
            newpos[iw]=W[iw]->R[iat] + 
	      m_sqrttau*delpos[iw] + oldScale[iw]*oldG[iw];
	    ratios[iw] = 1.0;
	  }
	  W.proposeMove_GPU(newpos, iat);
	  
          Psi.ratio(W,iat,ratios,newG, newL);
	  
	  // if (iat == 197) {
	  //   fprintf (stderr, "oldG[0] = %16.8f %16.8f %16.8f\n",
	  // 	     oldG[0][0], oldG[0][1], oldG[0][2]);
	  //   fprintf (stderr, "newG[0] = %16.8f %16.8f %16.8f\n",
	  // 	     newG[0][0], newG[0][1], newG[0][2]);
	  // }

          accepted.clear();
	  vector<bool> acc(nw, false);
          for(int iw=0; iw<nw; ++iw) {
	    PosType drOld = 
	      newpos[iw] - (W[iw]->R[iat] + oldScale[iw]*oldG[iw]);
	    //RealType logGf = -m_oneover2tau * dot(drOld, drOld);
	    RealType logGf = -0.5*dot(delpos[iw],delpos[iw]);			  
	    newScale[iw]   = getDriftScale(m_tauovermass,newG[iw]);
	    newScale[iw] = 0.5*Tau;
	    PosType drNew  = 
	      (newpos[iw] + newScale[iw]*newG[iw]) - W[iw]->R[iat];
	    RealType logGb = -m_oneover2tau * dot(drNew, drNew);
	    // cerr << "dotold = " << dot(drOld, drOld) << endl;	    
	    // cerr << "dotnew = " << dot(drNew, drNew) << endl;
	    RealType prob  = ratios[iw]*ratios[iw] * std::exp(std::sqrt(std::sqrt(2.0))*(logGb-logGf));
	    
            if(Random() < prob) {
              accepted.push_back(W[iw]);
	      nAccept++;
	      W[iw]->R[iat] = newpos[iw];
	      acc[iw] = true;
	    }
	    else 
	      nReject++;
	  }
	  W.acceptMove_GPU(acc);
	  if (accepted.size())
	    Psi.update(accepted,iat);
	}

	Psi.gradLapl(W, grad, lapl);
	H.evaluate (W, LocalEnergy);
	Estimators->accumulate(W);
      } while(step<nSteps);
      Psi.recompute(W);
      
      double accept_ratio = (double)nAccept/(double)(nAccept+nReject);
      Estimators->stopBlock(accept_ratio);

      nAcceptTot += nAccept;
      nRejectTot += nReject;
      ++block;
      
      recordBlock(block);

      clock_t block_end = clock();
      double block_time = (double)(block_end-block_start)/CLOCKS_PER_SEC;
      
    } while(block<nBlocks);
    //finalize a qmc section
    return finalize(block);
  }





  void VMCcuda::resetRun()
  {
    SpeciesSet tspecies(W.getSpeciesSet());
    int massind=tspecies.addAttribute("mass");
    RealType mass = tspecies(massind,0);
    RealType oneovermass = 1.0/mass;
    RealType oneoversqrtmass = std::sqrt(oneovermass);
    m_oneover2tau = 0.5*mass/Tau;
    m_sqrttau = std::sqrt(Tau/mass);
    m_tauovermass = Tau/mass;

    // Compute the size of data needed for each walker on the GPU card
    PointerPool<Walker_t::cuda_Buffer_t > pool;
    Psi.reserve (pool);
    app_log() << "Each walker requires " << pool.getTotalSize() * sizeof(CudaRealType)
	      << " bytes in GPU memory.\n";

    // Now allocate memory on the GPU card for each walker
    for (int iw=0; iw<W.WalkerList.size(); iw++) {
      Walker_t &walker = *(W.WalkerList[iw]);
      pool.allocate(walker.cuda_DataSet);
    }
    W.copyWalkersToGPU();
    W.updateLists_GPU();
    vector<RealType> logPsi(W.WalkerList.size(), 0.0);
    Psi.evaluateLog(W, logPsi);
    Estimators->start(nBlocks, true);
  }

  bool 
  VMCcuda::put(xmlNodePtr q){
    //nothing to add
    return true;
  }
}

/***************************************************************************
 * $RCSfile: VMCParticleByParticle.cpp,v $   $Author: jnkim $
 * $Revision: 1.25 $   $Date: 2006/10/18 17:03:05 $
 * $Id: VMCParticleByParticle.cpp,v 1.25 2006/10/18 17:03:05 jnkim Exp $ 
 ***************************************************************************/
