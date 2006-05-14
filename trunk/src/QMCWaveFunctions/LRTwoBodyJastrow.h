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
#ifndef QMCPLUSPLUS_LR_RPAJASTROW_H
#define QMCPLUSPLUS_LR_RPAJASTROW_H

#include "QMCWaveFunctions/OrbitalBase.h"

namespace qmcplusplus {

  class LRTwoBodyJastrow: public  OrbitalBase {
    
    ///pointer to the target ParticleSet
    ParticleSet* PtclRef;

    ///Coefficients
    Vector<RealType> Fk; 

  public:

    LRTwoBodyJastrow(ParticleSet& p);

    void reset();

    //evaluate the distance table with els
    void resetTargetParticleSet(ParticleSet& P);

    ValueType evaluateLog(ParticleSet& P,
		         ParticleSet::ParticleGradient_t& G, 
		         ParticleSet::ParticleLaplacian_t& L);

    inline ValueType evaluate(ParticleSet& P,
			      ParticleSet::ParticleGradient_t& G, 
			      ParticleSet::ParticleLaplacian_t& L) {
      return exp(evaluateLog(P,G,L));
    }

    ValueType ratio(ParticleSet& P, int iat);

    ValueType ratio(ParticleSet& P, int iat,
		    ParticleSet::ParticleGradient_t& dG,
		    ParticleSet::ParticleLaplacian_t& dL) ;

    ValueType logRatio(ParticleSet& P, int iat,
		    ParticleSet::ParticleGradient_t& dG,
		    ParticleSet::ParticleLaplacian_t& dL);

    void restore(int iat);
    void acceptMove(ParticleSet& P, int iat);
    void update(ParticleSet& P, 
		ParticleSet::ParticleGradient_t& dG, 
		ParticleSet::ParticleLaplacian_t& dL,
		int iat);


    ValueType registerData(ParticleSet& P, PooledData<RealType>& buf);
    ValueType updateBuffer(ParticleSet& P, PooledData<RealType>& buf);
    void copyFromBuffer(ParticleSet& P, PooledData<RealType>& buf);
    ValueType evaluate(ParticleSet& P, PooledData<RealType>& buf);
  };
}
#endif
