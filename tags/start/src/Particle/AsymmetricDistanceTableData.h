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
#ifndef OHMMS_QMC_ASYMMETRICDISTANCETABLEDATAIMPL_H
#define OHMMS_QMC_ASYMMETROCDISTANCETABLEDATAIMPL_H

namespace ohmmsqmc {
  
  /** A derived classe from DistacneTableData, specialized for dense-asymmetric case,
   * i.e., the source and target are distinct.
   *@todo Template with the boundary conditions
   */
  template<class BC>
  struct AsymmetricDTD: public DistanceTableData {

    const ParticleSet& Target;

    AsymmetricDTD(const ParticleSet& source, 
		  const ParticleSet& target): DistanceTableData(source,target),
					       Target(target){

    }

    void create(int walkers){
      int nw = (walkers>0)? walkers:1;
      reset(Origin.getTotalNum(),Target.getTotalNum(),nw);
    }

    /*!\fn  void reset(int n1, int n2, int nactive){
     *\param n1 the number of sources
     *\param n2 the number of targets
     *\param nactive the number of copies of the targets
     *\brief Resize the internal data and assign indices
     */
     inline void reset(int n1, int n2, int nactive){
      if( n1!=N[SourceIndex] || n2 != N[VisitorIndex] 
	  || nactive != N[WalkerIndex]) {
	N[SourceIndex] = n1;
	N[VisitorIndex] = n2;
	int m = n1*n2;
	if(m) { 
	  M.resize(n1+1);
	  J.resize(m);
	  PairID.resize(m);
	  resize(m,nactive);
	  M[0] = 0; int ij = 0;
	  for(int i=0; i<n1; i++) {
	    for(int j=0; j<n2; j++, ij++) {
	      J[ij] = j;
	      PairID[ij] = Origin.GroupID[i];
	    }
	    M[i+1] = M[i]+n2;
	  }
	  npairs_m = n1*n2;
	}
      }
    }

    ///evaluate the Distance Table using a set of Particle Positions
    inline void evaluate(const WalkerSetRef& W) {
      int copies = W.walkers();
      int visitors = W.particles();
      int ns = Origin.getTotalNum();

      reset(ns,visitors,copies);
      for(int iw=0; iw<copies; iw++) {
	int nn=0;
	for(int i=0; i<ns; i++) {
	  PosType r0 = Origin.R(i);
	  for(int j=0; j<visitors; j++,nn++) {
	    PosType drij = W.R(iw,j)-r0;
	    RealType sep = sqrt(BC::apply(Origin.Lattice,drij));
#ifdef USE_FASTWALKER
	    r2_m(nn,iw) = sep;
	    rinv2_m(nn,iw) = 1.0/sep;
	    dr2_m(nn,iw) = drij;
#else
	    r2_m(iw,nn) = sep;
	    rinv2_m(iw,nn) = 1.0/sep;
	    dr2_m(iw,nn) = drij;
#endif
	  }
	}
      }
    }

    ///not so useful inline but who knows
    inline void evaluate(const ParticleSet& P){
      //reset(Origin.getTotalNum(),P.getTotalNum(),1);
      int nn=0;
      for(int i=0; i<N[SourceIndex]; i++) {
	PosType r0 = Origin.R[i];
	for(int j=0; j<N[VisitorIndex]; j++,nn++) {
	  PosType drij = P.R[j]-r0;
	  RealType sep = sqrt(BC::apply(Origin.Lattice,drij));
	  r_m[nn]    = sep;
	  rinv_m[nn] = 1.0/sep;
	  dr_m[nn]   = drij;
	}
      }
    }


    inline void update(const ParticleSet& P, IndexType iat) {
      IndexType nn = iat;
      for(int i=0; i<N[SourceIndex]; i++, nn+=N[VisitorIndex]) {
	PosType drij = P.R[iat]-Origin.R[i];
	RealType sep = sqrt(BC::apply(Origin.Lattice,drij));
	r_m[nn]    = sep;
	rinv_m[nn] = 1.0/sep;
	dr_m[nn]  = drij;
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
