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
#include "QMCDrivers/DMC/DMC.h"
#include "QMCDrivers/DMC/DMCUpdateAll.h"
#include "QMCDrivers/DMC/DMCUpdatePbyP.h"
#include "QMCDrivers/DMC/DMCNonLocalUpdate.h"
#include "Estimators/DMCEnergyEstimator.h"
#include "QMCApp/HamiltonianPool.h"
#include "Message/Communicate.h"
#include "Message/OpenMP.h"

namespace qmcplusplus { 

  /// Constructor.
  DMC::DMC(MCWalkerConfiguration& w, TrialWaveFunction& psi, QMCHamiltonian& h):
    QMCDriver(w,psi,h), KillNodeCrossing(0), Reconfiguration("no"), Mover(0)
  {
    RootName = "dummy";
    QMCType ="DMC";

    QMCDriverMode.set(QMC_UPDATE_MODE,1);
    QMCDriverMode.set(QMC_MULTIPLE,1);

    m_param.add(KillWalker,"killnode","string");
    m_param.add(BenchMarkRun,"benchmark","string");
    m_param.add(Reconfiguration,"reconfiguration","string");

    Estimators = new ScalarEstimatorManager(H);
    Estimators->add(new DMCEnergyEstimator,"elocal");
  }

  bool DMC::run() {

    resetUpdateEngine();

    //set the collection mode for the estimator
    Estimators->setCollectionMode(branchEngine->SwapMode);
    Estimators->reportHeader(AppendRun);
    Estimators->reset();

    IndexType block = 0;
    IndexType CurrentStep = 0;
    do // block
    {
      Estimators->startBlock();
      Mover->startBlock();
      IndexType step = 0;
      do  //step
      {
        IndexType interval = 0;
        do // interval
        {
          Mover->advanceWalkers(W.begin(),W.end());
          ++interval;
        } while(interval<BranchInterval);
        Mover->setMultiplicity(W.begin(),W.end());
        Estimators->accumulate(W);
        branchEngine->branch(CurrentStep,W);
        ++step; 
        CurrentStep+=BranchInterval;
      } while(step<nSteps);

      Estimators->stopBlock(Mover->acceptRatio());

      block++;
      recordBlock(block);

      if(QMCDriverMode[QMC_UPDATE_MODE] && CurrentStep%100 == 0) 
        Mover->updateWalkers(W.begin(), W.end());

    } while(block<nBlocks);

    return finalize(block);
  }

  void DMC::resetUpdateEngine() {

    if(Mover==0) {
      if(QMCDriverMode[QMC_UPDATE_MODE])
      {
        if(NonLocalMove == "yes")
        {
          DMCNonLocalUpdatePbyP* nlocMover= new DMCNonLocalUpdatePbyP(W,Psi,H,Random); 
          nlocMover->put(qmcNode);
          Mover=nlocMover;
        } 
        else
        {
          Mover= new DMCUpdatePbyPWithRejection(W,Psi,H,Random); 
        }
        Mover->resetRun(branchEngine);
        Mover->initWalkersForPbyP(W.begin(),W.end());
      } 
      else
      {
        if(NonLocalMove == "yes") {
          app_log() << "  Non-local update is used." << endl;
          DMCNonLocalUpdate* nlocMover= new DMCNonLocalUpdate(W,Psi,H,Random);
          nlocMover->put(qmcNode);
          Mover=nlocMover;
        } else {
          if(KillNodeCrossing) {
            Mover = new DMCUpdateAllWithKill(W,Psi,H,Random);
          } else {
            Mover = new DMCUpdateAllWithRejection(W,Psi,H,Random);
          }
        }
        Mover->resetRun(branchEngine);
        Mover->initWalkers(W.begin(),W.end());
      }
    }

    bool fixW=(Reconfiguration == "yes");
    if(fixW)
    {
      if(QMCDriverMode[QMC_UPDATE_MODE])
        app_log() << "  DMC PbyP Update with reconfigurations" << endl;
      else
        app_log() << "  DMC walker Update with reconfigurations" << endl;

      Mover->MaxAge=0;
      if(BranchInterval<0)
      {
        BranchInterval=nSteps;
      }
    } 
    else 
    {
      if(QMCDriverMode[QMC_UPDATE_MODE])
      {
        app_log() << "  DMC PbyP Update with a fluctuating population" << endl;
        Mover->MaxAge=1;
      }
      else
      {
        app_log() << "  DMC walker Update with a fluctuating population" << endl;
        Mover->MaxAge=3;
      }
      if(BranchInterval<0) BranchInterval=1;
    }

    branchEngine->initWalkerController(Tau,fixW);
  }

  bool DMC::put(xmlNodePtr q){
    return true;
  }

}

/***************************************************************************
 * $RCSfile: DMC.cpp,v $   $Author: jnkim $
 * $Revision: 1592 $   $Date: 2007-01-04 16:48:00 -0600 (Thu, 04 Jan 2007) $
 * $Id: DMC.cpp 1592 2007-01-04 22:48:00Z jnkim $ 
 ***************************************************************************/
