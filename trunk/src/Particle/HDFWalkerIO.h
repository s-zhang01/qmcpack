//////////////////////////////////////////////////////////////////
// (c) Copyright 2004- by Jeongnim Kim and Jordan Vincent
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
//   Department of Physics, Ohio State University
//   Ohio Supercomputer Center
//////////////////////////////////////////////////////////////////
// -*- C++ -*-
#ifndef QMCPLUSPLUS_WALKER_IO_H
#define QMCPLUSPLUS_WALKER_IO_H

#include "OhmmsData/HDFAttribIO.h"

namespace qmcplusplus {

  /** Writes a set of walker configurations to an HDF5 file. */

  class HDFWalkerOutput {

    bool AppendMode;
    ///number of times file has been written to
    int Counter;
    ///id for HDF5 file 
    hid_t h_file;
    ///id for HDF5 main group 
    hid_t h_config;
    ///id for the random number generator
    hid_t h_random;
  public:

    HDFWalkerOutput(const string& fname, bool append=true, int count=0);
    ~HDFWalkerOutput();
    bool get(MCWalkerConfiguration&);

    template<class CT>
    void write(CT& anything) {
      anything.write(h_config);
    }

    /** return the file ID **/
    hid_t getFileID() { return h_file;}

    /** return the config_collection file ID **/
    hid_t getConfigID() { return h_config;}
  };

  /** Reads a set of walker configurations from an HDF5 file. */

  class HDFWalkerInput {

    ///id of the first set
    int FirstSet;
    ///id of the last set
    int LastSet;
    ///number of times file has been accesed
    int Counter;
    ///number of sets of walker configurations
    int NumSets;
    ///id for HDF5 file 
    hid_t h_file;
    ///id for HDF5 main group 
    hid_t h_config;

  public:

    HDFWalkerInput(const string& fname, int ipart=0, int nparts=1);
    ~HDFWalkerInput();
    //int put(MCWalkerConfiguration&);
    bool put(MCWalkerConfiguration&, int ic);

    bool append(MCWalkerConfiguration& w);
    bool append(MCWalkerConfiguration& w, int nwalkers);

    template<class CT>
    void read(CT& anything) {
      anything.read(h_config);
    }


    /** read the state of the number generator
     * @param restart if true, read the state
     */
    void getRandomState(bool restart);

  };

}
#endif
/***************************************************************************
 * $RCSfile$   $Author$
 * $Revision$   $Date$
 * $Id$ 
 ***************************************************************************/
