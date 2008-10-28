template<typename T, int BS>
__device__
T min_dist (T x, T y, T z, 
	    T L[3][3], T Linv[3][3], T images[27][3])
{
  int tid = threadIdx.x;
  T u0 = Linv[0][0]*x + Linv[0][1]*y + Linv[0][2]*z;  
  T u1 = Linv[1][0]*x + Linv[1][1]*y + Linv[1][2]*z;
  T u2 = Linv[2][0]*x + Linv[2][1]*y + Linv[2][2]*z;

  u0 -= rintf(u0);
  u1 -= rintf(u1);
  u2 -= rintf(u2);

  x = L[0][0]*u0 + L[0][1]*u1 + L[0][2]*u2;
  y = L[1][0]*u0 + L[1][1]*u1 + L[1][2]*u2;
  z = L[2][0]*u0 + L[2][1]*u1 + L[2][2]*u2;

  __shared__ T dist2[27];
  dist2[tid] = 1.0e8;
  if (tid < 27) {
    x += images[tid][0];
    y += images[tid][1];
    z += images[tid][2];
    dist2[tid] = x*x + y*y + z*z;
  }
  __syncthreads();
  for (int s=BS>>1; s>0; s>>=1) {
    if (tid < s)
      dist2[tid] = (dist2[tid+s] < dist2[tid]) ? dist2[tid+s] : dist2[tid];
    __syncthreads();
  }

  return sqrtf(dist2[0]);
}




template<typename T, int BS>
__global__ void
find_core_electrons_kernel(T *R[], int numElec,
			   T I[], int firstIon, int lastIon,
			   T rcut, T L[3][3], T Linv[3][3],
			   int2 *pairs[], T *dist[], int numPairs[])
{
  int tid = threadIdx.x;
  __shared__ T images[27][3];
  __shared__ T *myR, *mydist;
  __shared__ int2 *mypairs;
  if (tid == 0) {
    myR     =     R[blockIdx.x];
    mydist  =  dist[blockIdx.x];
    mypairs = pairs[blockIdx.x];
  }

  int i0 = tid / 9;
  int i1 = (tid - 9*i0)/3;
  int i2 = (tid - 9*i0 - 3*i1);
  if (tid < 27) {
    images[tid][0] = (T)i0*L[0][0] + (T)i1*L[1][0] + (T)i2*L[2][0];
    images[tid][1] = (T)i0*L[0][1] + (T)i1*L[1][1] + (T)i2*L[2][1];
    images[tid][2] = (T)i0*L[0][2] + (T)i1*L[1][2] + (T)i2*L[2][2];
  }
  __syncthreads();


  int numIon = lastIon - firstIon + 1;
  int numElecBlocks = numElec/BS + ((numElec % BS) ? 1 : 0);
  int numIonBlocks  = numIon /BS + ((numIon  % BS) ? 1 : 0);

  __shared__ T r[BS][3];
  __shared__ T i[BS][3];
  __shared__ int2 blockpairs[BS];
  __shared__ T blockdist[BS];
  int npairs=0, index=0, blockNum=0;


  for (int iBlock=0; iBlock<numIonBlocks; iBlock++) {
    for (int dim=0; dim<3; dim++) 
      if (dim*BS+tid < 3*numIon)
	i[0][dim*BS+tid] = I[3*BS*iBlock + 3*firstIon + dim*BS+tid];
    int ionEnd = ((iBlock+1)*BS < numIon) ? BS : (numIon - iBlock*BS);

    for (int eBlock=0; eBlock<numElecBlocks; eBlock++) {
      int elecEnd = ((eBlock+1)*BS < numElec) ? BS : (numElec - eBlock*BS);
      for (int dim=0; dim<3; dim++) 
	if (dim*BS+tid < 3*numElec)
	  r[0][dim*BS+tid] = myR[3*BS*eBlock + dim*BS+tid];
      for (int ion=0; ion<ionEnd; ion++)
	for (int elec=0; elec<elecEnd; elec++) {
	  T dist = min_dist<T,BS>(r[elec][0]-i[ion][0], r[elec][1]-i[ion][1],
				  r[elec][2]-i[ion][2], L, Linv, images);
	  if (dist < rcut) {
	    if (index < BS) {
	      if (tid == 0) {
		blockpairs[index].x = iBlock*BS+ion;
		blockpairs[index].y = eBlock*BS+elec;
		blockdist[index]    = dist;
		index++;
	      }
	    }
	    else {
	      mypairs[blockNum*BS+tid] = blockpairs[tid];
	      mydist[blockNum*BS+tid]  = blockdist[tid];
	      blockNum++;
	      index = 0;
	    }
	    npairs++;
	  }
	}
    }
    
  }
  // Write pairs and distances remaining the final block
  if (tid < index) {
    mypairs[blockNum*BS+tid] = blockpairs[tid];
    mydist[blockNum*BS+tid]  = blockdist[tid];
  }
  if (tid == 0)
    numPairs[blockIdx.x] = npairs;
}



void
find_core_electrons (float *R[], int numElec, 
		     float I[], int firstIon, int lastIon,
		     float rcut, float L[3][3], float Linv[3][3], 
		     int2 *pairs[], float *dist[], 
		     int numPairs[], int numWalkers)
{
  const int BS = 32;
  
  dim3 dimBlock(BS);
  dim3 dimGrid(numWalkers);
  
  find_core_electrons_kernel<float,BS><<<dimGrid,dimBlock>>> 
    (R, numElec, I, firstIon, lastIon, rcut, L, Linv, pairs, dist, numPairs);
}



// Maximum quadrature points of 32;

template<typename T, int BS>
__global__ void
make_work_list_kernel (int2 *pairs[], int numpairs,
		       T I[], T quadpoints[], int numquadpoints,
		       T *ratio_pos[])
{
  __shared__ T qp[BS][3];
  
  int tid = threadIdx.x;
  for (int i=0; i<3; i++)
    if (tid*i*BS < 3*BS)
      qp[0][i*BS + tid] = quadpoints[i*BS + tid];
  __syncthreads();

  


}
