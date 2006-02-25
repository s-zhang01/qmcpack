//////////////////////////////////////////////////////////////////
// (c) Copyright 2005- by Jeongnim Kim
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   Jeongnim Kim
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
#ifndef QMCPLUSPLUS_RECONFIGURATION_WALKER_CONTROLMPI_H
#define QMCPLUSPLUS_RECONFIGURATION_WALKER_CONTROLMPI_H

#include "QMCDrivers/WalkerControlBase.h"

namespace qmcplusplus {

  /** Class to handle walker controls with simple global sum
   *
   * Base class to handle serial mode with branching only
   */
  struct WalkerReconfigurationMPI: public WalkerControlBase {

    ///number of mpi contexts
    int NumContexts;
    /// context on a node
    int MyContext;
    ///total number of walkers 
    int TotalWalkers;
    ///starting index of the local walkers
    int FirstWalker;
    ///ending index of the local walkers
    int LastWalker;
    //random number [0,1)
    RealType UnitZeta;
    //random number [0,1)/number of walkers
    RealType DeltaStep;

    //vector<int>      IndexCopy;
    //weight per walker
    vector<RealType> wConf;
    //accumulated weight [0,ip) for each ip
    vector<RealType> wOffset;
    //local sum of the weights for each ip
    vector<RealType> wSum;
    //comb
    //vector<RealType> Zeta;

    /** default constructor
     *
     * Set the SwapMode to zero so that instantiation can be done
     */
    WalkerReconfigurationMPI();

    /** perform branch and swap walkers as required */
    int branch(int iter, MCWalkerConfiguration& W, RealType trigger);

    /** return the surviving Walkers
     */
    int swapWalkers(MCWalkerConfiguration& W);
  };
}
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/

