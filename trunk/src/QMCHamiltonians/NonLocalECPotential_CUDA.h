//////////////////////////////////////////////////////////////////
// (c) Copyright 2010-  by Ken Esler and Jeongnim Kim
//////////////////////////////////////////////////////////////////
//   National Center for Supercomputing Applications &
//   Materials Computation Center
//   University of Illinois, Urbana-Champaign
//   Urbana, IL 61801
//   e-mail: esler@uiuc.edu
//
// Supported by 
//   National Center for Supercomputing Applications, UIUC
//   Materials Computation Center, UIUC
//////////////////////////////////////////////////////////////////
#ifndef QMCPLUSPLUS_NONLOCALECPOTENTIAL_CUDA_H
#define QMCPLUSPLUS_NONLOCALECPOTENTIAL_CUDA_H

#include "NonLocalECPotential.h"

namespace qmcplusplus {

  class NonLocalECPotential_CUDA: public NonLocalECPotential
  {
  protected:    
    //////////////////////////////////
    // Vectorized evaluation on GPU //
    //////////////////////////////////
    int NumIonGroups;
    vector<int> IonFirst, IonLast;
    gpu::device_vector<CUDA_PRECISION> Ions_GPU, L, Linv;
    gpu::device_vector<int> Elecs_GPU;
    gpu::host_vector<int> Elecs_host;
    gpu::device_vector<CUDA_PRECISION> Dist_GPU;
    gpu::host_vector<CUDA_PRECISION> Dist_host;
    gpu::device_vector<int*> Eleclist_GPU;
    gpu::device_vector<CUDA_PRECISION*> Distlist_GPU;
    gpu::device_vector<int> NumPairs_GPU;
    gpu::host_vector<int> NumPairs_host;
    int NumElecs;
    // The maximum number of quadrature points over all the ions species
    int MaxKnots, MaxPairs, RatiosPerWalker;
    // These are the positions at which we have to evalate the WF ratios
    // It has size OHMMS_DIM * MaxPairs * MaxKnots * NumWalkers
    gpu::device_vector<CUDA_PRECISION> RatioPos_GPU, CosTheta_GPU;
    gpu::host_vector<CUDA_PRECISION> RatioPos_host, CosTheta_host;
    gpu::device_vector<CUDA_PRECISION*> RatioPoslist_GPU, CosThetalist_GPU;

    // Quadrature points
    vector<gpu::device_vector<CUDA_PRECISION> > QuadPoints_GPU;
    vector<std::vector<CUDA_PRECISION> > QuadPoints_host;
    int CurrentNumWalkers;

    // These are used in calling Psi->NLratios
    vector<NLjob> JobList;
    vector<PosType> QuadPosList;
    vector<ValueType> RatioList;
    

    vector<PosType> SortedIons;

    void setupCUDA(ParticleSet &elecs);
    void resizeCUDA(int nw);

  public:
    NonLocalECPotential_CUDA(ParticleSet& ions, ParticleSet& els, 
			     TrialWaveFunction& psi, bool doForces=false);

    QMCHamiltonianBase* makeClone(ParticleSet& qp, TrialWaveFunction& psi);

    void addEnergy(MCWalkerConfiguration &W, vector<RealType> &LocalEnergy);
    void addEnergy(MCWalkerConfiguration &W, vector<RealType> &LocalEnergy,
		   vector<vector<NonLocalData> > &Txy);
  };
  

}

#endif