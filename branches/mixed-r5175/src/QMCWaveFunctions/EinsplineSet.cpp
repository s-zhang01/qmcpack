//////////////////////////////////////////////////////////////////
// (c) Copyright 2006-  by Jeongnim Kim and Ken Esler           //
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
//   National Center for Supercomputing Applications &          //
//   Materials Computation Center                               //
//   University of Illinois, Urbana-Champaign                   //
//   Urbana, IL 61801                                           //
//   e-mail: jnkim@ncsa.uiuc.edu                                //
//                                                              //
// Supported by                                                 //
//   National Center for Supercomputing Applications, UIUC      //
//   Materials Computation Center, UIUC                         //
//////////////////////////////////////////////////////////////////

#include "QMCWaveFunctions/EinsplineSet.h"
#include <einspline/multi_bspline.h>
#include "Configuration.h"
#ifdef HAVE_MKL
  #include <mkl_vml.h>
#endif

namespace qmcplusplus {

  EinsplineSet::UnitCellType
  EinsplineSet::GetLattice()
  {
    return SuperLattice;
  }
  
  void
  EinsplineSet::resetTargetParticleSet(ParticleSet& e)
  {
  }

  void
  EinsplineSet::resetSourceParticleSet(ParticleSet& ions)
  {
  }
  
  void
  EinsplineSet::setOrbitalSetSize(int norbs)
  {
    OrbitalSetSize = norbs;
  }
  
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::resetParameters(const opt_variables_type& active)
  {

  }

  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::resetTargetParticleSet(ParticleSet& e)
  {
  }

  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::setOrbitalSetSize(int norbs)
  {
    OrbitalSetSize = norbs;
  }
  
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, RealValueVector_t& psi)
  {
    ValueTimer.start();
    PosType r (P.R[iat]);

    // Do core states first
    int icore = NumValenceOrbs;
    for (int tin=0; tin<MuffinTins.size(); tin++) {
      MuffinTins[tin].evaluateCore(r, StorageValueVector, icore);
      icore += MuffinTins[tin].get_num_core();
    }
    // Add phase to core orbitals
    for (int j=NumValenceOrbs; j<StorageValueVector.size(); j++) {
      PosType k = kPoints[j];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      StorageValueVector[j] *= e_mikr;
    }

    // Check if we are inside a muffin tin.  If so, compute valence
    // states in the muffin tin.
    bool inTin = false;
    bool need2blend = false;
    double b(0.0);
    for (int tin=0; tin<MuffinTins.size() && !inTin; tin++) {
      MuffinTins[tin].inside(r, inTin, need2blend);
      if (inTin) {
	MuffinTins[tin].evaluate (r, StorageValueVector);
	if (need2blend) {
	  PosType disp = MuffinTins[tin].disp(r);
	  double dr = std::sqrt(dot(disp, disp));
	  MuffinTins[tin].blend_func(dr, b);
	}
	break;
      }
    }

    // Check atomic orbitals
    bool inAtom = false;
    for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
      inAtom = AtomicOrbitals[jat].evaluate (r, StorageValueVector);
      if (inAtom) break;
    }
      
    StorageValueVector_t &valVec = 
      need2blend ? BlendValueVector : StorageValueVector;

    if (!inTin || need2blend) {
      if (!inAtom) {
	PosType ru(PrimLattice.toUnit(P.R[iat]));
	for (int i=0; i<OHMMS_DIM; i++)
	  ru[i] -= std::floor (ru[i]);
	EinsplineTimer.start();
	EinsplineMultiEval (MultiSpline, ru, valVec);
	EinsplineTimer.stop();
      
	// Add e^ikr phase to B-spline orbitals
	for (int j=0; j<NumValenceOrbs; j++) {
	  PosType k = kPoints[j];
	  double s,c;
	  double phase = -dot(r, k);
	  sincos (phase, &s, &c);
	  complex<double> e_mikr (c,s);
	  valVec[j] *= e_mikr;
	}
      }
    }
    int N = StorageValueVector.size();

    // If we are in a muffin tin, don't add the e^ikr term
    // We should add it to the core states, however

    if (need2blend) {
      int psiIndex = 0;
      for (int j=0; j<N; j++) {
	complex<double> psi1 = StorageValueVector[j];
	complex<double> psi2 =   BlendValueVector[j];
	
	complex<double> psi_val = b * psi1 + (1.0-b) * psi2;

	psi[psiIndex] = real(psi_val);
	psiIndex++;
	if (MakeTwoCopies[j]) {
	  psi[psiIndex] = imag(psi_val);
	  psiIndex++;
	}
      }

    }
    else {
      int psiIndex = 0;
      for (int j=0; j<N; j++) {
	complex<double> psi_val = StorageValueVector[j];
	psi[psiIndex] = real(psi_val);
	psiIndex++;
	if (MakeTwoCopies[j]) {
	  psi[psiIndex] = imag(psi_val);
	  psiIndex++;
	}
      }
    }
    ValueTimer.stop();
  }


  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, ComplexValueVector_t& psi)
  {
    ValueTimer.start();
    PosType r (P.R[iat]);
    PosType ru(PrimLattice.toUnit(P.R[iat]));
    for (int i=0; i<OHMMS_DIM; i++)
      ru[i] -= std::floor (ru[i]);
    EinsplineTimer.start();
    EinsplineMultiEval (MultiSpline, ru, StorageValueVector);
    EinsplineTimer.stop();
    //computePhaseFactors(r);
    for (int i=0; i<psi.size(); i++) {
      PosType k = kPoints[i];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      convert (e_mikr*StorageValueVector[i], psi[i]);
    }
    ValueTimer.stop();
  }

  // This is an explicit specialization of the above for real orbitals
  // with a real return value, i.e. simulations at the gamma or L 
  // point.
  template<> void
  EinsplineSetExtended<double>::evaluate
  (const ParticleSet &P, int iat, RealValueVector_t& psi)
  {
    ValueTimer.start();
    PosType r (P.R[iat]);

    bool inAtom = false;
    for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
      inAtom = AtomicOrbitals[jat].evaluate (r, psi);
      if (inAtom) 
	break;
    }

    if (!inAtom) {
      PosType ru(PrimLattice.toUnit(P.R[iat]));
      int sign=0;
      for (int i=0; i<OHMMS_DIM; i++) {
	RealType img = std::floor(ru[i]);
	ru[i] -= img;
	sign += HalfG[i] * (int)img;
      }
      // Check atomic orbitals
      EinsplineTimer.start();
      EinsplineMultiEval (MultiSpline, ru, psi);
      EinsplineTimer.stop();
    
      if (sign & 1) 
	for (int j=0; j<psi.size(); j++)
	  psi[j] *= -1.0;
    }
    ValueTimer.stop();
  }

  // template<> void
  // EinsplineSetExtended<double>::evaluate
  // (const ParticleSet &P, const PosType& r, vector<RealType> &psi)
  // {
  //   ValueTimer.start();
  //   PosType ru(PrimLattice.toUnit(r));
  //   int image[OHMMS_DIM];
  //   for (int i=0; i<OHMMS_DIM; i++) {
  //     RealType img = std::floor(ru[i]);
  //     ru[i] -= img;
  //     image[i] = (int) img;
  //   }
  //   EinsplineTimer.start();
  //   EinsplineMultiEval (MultiSpline, ru, psi);
  //   EinsplineTimer.stop();
  //   int sign=0;
  //   for (int i=0; i<OHMMS_DIM; i++) 
  //     sign += HalfG[i]*image[i];
  //   if (sign & 1) 
  //     for (int j=0; j<psi.size(); j++)
  // 	psi[j] *= -1.0;
  //   ValueTimer.stop();
  // }


  // template<> void
  // EinsplineSetExtended<complex<double> >::evaluate
  // (const ParticleSet &P, const PosType& r, vector<RealType> &psi)
  // {
  //   cerr << "Not Implemented.\n";
  // }


  // Value, gradient, and laplacian
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, RealValueVector_t& psi, 
   RealGradVector_t& dpsi, RealValueVector_t& d2psi)
  {
    VGLTimer.start();
    PosType r (P.R[iat]);
    complex<double> eye (0.0, 1.0);
    
    // Do core states first
    int icore = NumValenceOrbs;
    for (int tin=0; tin<MuffinTins.size(); tin++) {
      MuffinTins[tin].evaluateCore(r, StorageValueVector, StorageGradVector, 
				   StorageLaplVector, icore);
      icore += MuffinTins[tin].get_num_core();
    }
    
    // Add phase to core orbitals
    for (int j=NumValenceOrbs; j<StorageValueVector.size(); j++) {
      complex<double> u = StorageValueVector[j];
      TinyVector<complex<double>,OHMMS_DIM> gradu = StorageGradVector[j];
      complex<double> laplu = StorageLaplVector[j];
      PosType k = kPoints[j];
      TinyVector<complex<double>,OHMMS_DIM> ck;
      for (int n=0; n<OHMMS_DIM; n++)	  ck[n] = k[n];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      StorageValueVector[j] = e_mikr*u;
      StorageGradVector[j]  = e_mikr*(-eye*u*ck + gradu);
      StorageLaplVector[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);
    }

    // Check muffin tins;  if inside evaluate the orbitals
    bool inTin = false;
    bool need2blend = false;
    PosType disp;
    double b, db, d2b;
    for (int tin=0; tin<MuffinTins.size(); tin++) {
      MuffinTins[tin].inside(r, inTin, need2blend);
      if (inTin) {
	MuffinTins[tin].evaluate (r, StorageValueVector, StorageGradVector, StorageLaplVector);
	if (need2blend) {
	  disp = MuffinTins[tin].disp(r);
	  double dr = std::sqrt(dot(disp, disp));
	  MuffinTins[tin].blend_func(dr, b, db, d2b);
	}
	break;
      }
    }


    bool inAtom = false;
    for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
      inAtom = AtomicOrbitals[jat].evaluate 
	(r, StorageValueVector, StorageGradVector, StorageLaplVector);
      if (inAtom) break;
    }

    StorageValueVector_t &valVec =  
      need2blend ? BlendValueVector : StorageValueVector;
    StorageGradVector_t &gradVec =  
      need2blend ? BlendGradVector : StorageGradVector;
    StorageValueVector_t &laplVec =  
      need2blend ? BlendLaplVector : StorageLaplVector;

    // Otherwise, evaluate the B-splines
    if (!inTin || need2blend) {
      if (!inAtom) {
	PosType ru(PrimLattice.toUnit(P.R[iat]));
	for (int i=0; i<OHMMS_DIM; i++)
	  ru[i] -= std::floor (ru[i]);
	EinsplineTimer.start();
	EinsplineMultiEval (MultiSpline, ru, valVec, gradVec, StorageHessVector);
	EinsplineTimer.stop();
	for (int j=0; j<NumValenceOrbs; j++) {
	  gradVec[j] = dot (PrimLattice.G, gradVec[j]);
	  laplVec[j] = trace (StorageHessVector[j], GGt);
	}
      
	// Add e^-ikr phase to B-spline orbitals
	for (int j=0; j<NumValenceOrbs; j++) {
	  complex<double> u = valVec[j];
	  TinyVector<complex<double>,OHMMS_DIM> gradu = gradVec[j];
	  complex<double> laplu = laplVec[j];
	  PosType k = kPoints[j];
	  TinyVector<complex<double>,OHMMS_DIM> ck;
	  for (int n=0; n<OHMMS_DIM; n++)	  ck[n] = k[n];
	  double s,c;
	  double phase = -dot(r, k);
	  sincos (phase, &s, &c);
	  complex<double> e_mikr (c,s);
	  valVec[j]   = e_mikr*u;
	  gradVec[j]  = e_mikr*(-eye*u*ck + gradu);
	  laplVec[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);
	}
      }
    }

    // Finally, copy into output vectors
    int psiIndex = 0;
    int N = StorageValueVector.size();
    if (need2blend) {
      for (int j=0; j<NumValenceOrbs; j++) {
	complex<double> psi_val, psi_lapl;
	TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	PosType rhat = 1.0/std::sqrt(dot(disp,disp)) * disp;
	complex<double> psi1 = StorageValueVector[j];
	complex<double> psi2 =   BlendValueVector[j];
	TinyVector<complex<double>,OHMMS_DIM> dpsi1 = StorageGradVector[j];
	TinyVector<complex<double>,OHMMS_DIM> dpsi2 = BlendGradVector[j];
	complex<double> d2psi1 = StorageLaplVector[j];
	complex<double> d2psi2 =   BlendLaplVector[j];
	
	TinyVector<complex<double>,OHMMS_DIM> zrhat;
	for (int i=0; i<OHMMS_DIM; i++)
	  zrhat[i] = rhat[i];

	psi_val  = b * psi1 + (1.0-b)*psi2;
	psi_grad = b * dpsi1 + (1.0-b)*dpsi2 + db * (psi1 - psi2)* zrhat;
	psi_lapl = b * d2psi1 + (1.0-b)*d2psi2 +
	  2.0*db * (dot(zrhat,dpsi1) - dot(zrhat, dpsi2)) +
	  d2b * (psi1 - psi2);
	
	psi[psiIndex] = real(psi_val);
	for (int n=0; n<OHMMS_DIM; n++)
	  dpsi[psiIndex][n] = real(psi_grad[n]);
	d2psi[psiIndex] = real(psi_lapl);
	psiIndex++;
	if (MakeTwoCopies[j]) {
	  psi[psiIndex] = imag(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi[psiIndex][n] = imag(psi_grad[n]);
	  d2psi[psiIndex] = imag(psi_lapl);
	  psiIndex++;
	}
      } 
      for (int j=NumValenceOrbs; j<N; j++) {
	complex<double> psi_val, psi_lapl;
	TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	psi_val  = StorageValueVector[j];
	psi_grad = StorageGradVector[j];
	psi_lapl = StorageLaplVector[j];
	
	psi[psiIndex] = real(psi_val);
	for (int n=0; n<OHMMS_DIM; n++)
	  dpsi[psiIndex][n] = real(psi_grad[n]);
	d2psi[psiIndex] = real(psi_lapl);
	psiIndex++;
	if (MakeTwoCopies[j]) {
	  psi[psiIndex] = imag(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi[psiIndex][n] = imag(psi_grad[n]);
	  d2psi[psiIndex] = imag(psi_lapl);
	  psiIndex++;
	}
      }
    }
    else {
      for (int j=0; j<N; j++) {
	complex<double> psi_val, psi_lapl;
	TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	psi_val  = StorageValueVector[j];
	psi_grad = StorageGradVector[j];
	psi_lapl = StorageLaplVector[j];
	
	psi[psiIndex] = real(psi_val);
	for (int n=0; n<OHMMS_DIM; n++)
	  dpsi[psiIndex][n] = real(psi_grad[n]);
	d2psi[psiIndex] = real(psi_lapl);
	psiIndex++;
	if (MakeTwoCopies[j]) {
	  psi[psiIndex] = imag(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi[psiIndex][n] = imag(psi_grad[n]);
	  d2psi[psiIndex] = imag(psi_lapl);
	  psiIndex++;
	}
      }
    }
    VGLTimer.stop();
  }


  // Value, gradient, and laplacian
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, RealValueVector_t& psi,
   RealGradVector_t& dpsi, RealHessVector_t& grad_grad_psi)
  {
    APP_ABORT("need specialization for HessVector in EinsplineSet");
// something is wrong below
/*
    VGLTimer.start();
    PosType r (P.R[iat]);
    complex<double> eye (0.0, 1.0);

    // Do core states first
    int icore = NumValenceOrbs;
    for (int tin=0; tin<MuffinTins.size(); tin++) {
      //APP_ABORT("MuffinTins not implemented with Hessian evaluation.\n");
      MuffinTins[tin].evaluateCore(r, StorageValueVector, StorageGradVector, 
                                   StorageLaplVector, icore);
      icore += MuffinTins[tin].get_num_core();
    }

    // Add phase to core orbitals
    for (int j=NumValenceOrbs; j<StorageValueVector.size(); j++) {
      complex<double> u = StorageValueVector[j];
      TinyVector<complex<double>,OHMMS_DIM> gradu = StorageGradVector[j];
      complex<double> laplu = StorageLaplVector[j];
      PosType k = kPoints[j];
      TinyVector<complex<double>,OHMMS_DIM> ck;
      for (int n=0; n<OHMMS_DIM; n++)     ck[n] = k[n];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      StorageValueVector[j] = e_mikr*u;
      StorageGradVector[j]  = e_mikr*(-eye*u*ck + gradu);
      StorageLaplVector[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);
    }

    // Check muffin tins;  if inside evaluate the orbitals
    bool inTin = false;
    bool need2blend = false;
    PosType disp;
    double b, db, d2b;
    for (int tin=0; tin<MuffinTins.size(); tin++) {
      APP_ABORT("MuffinTins not implemented with Hessian evaluation.\n");
      MuffinTins[tin].inside(r, inTin, need2blend);
      if (inTin) {
        MuffinTins[tin].evaluate (r, StorageValueVector, StorageGradVector, StorageLaplVector);
        if (need2blend) {
          disp = MuffinTins[tin].disp(r);
          double dr = std::sqrt(dot(disp, disp));
          MuffinTins[tin].blend_func(dr, b, db, d2b);
        }
        break;
      }
    }


    bool inAtom = false;
    for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
      inAtom = AtomicOrbitals[jat].evaluate
        (r, StorageValueVector, StorageGradVector, StorageLaplVector);
      if (inAtom) break;
    }

    StorageValueVector_t &valVec =
      need2blend ? BlendValueVector : StorageValueVector;
    StorageGradVector_t &gradVec =
      need2blend ? BlendGradVector : StorageGradVector;
    StorageHessVector_t &hessVec =
      need2blend ? BlendHessVector : StorageHessVector;
    Tensor<complex<double>,OHMMS_DIM> tmphs;
    // Otherwise, evaluate the B-splines
    if (!inTin || need2blend) {
      if (!inAtom) {
        PosType ru(PrimLattice.toUnit(P.R[iat]));
        for (int i=0; i<OHMMS_DIM; i++)
          ru[i] -= std::floor (ru[i]);
        EinsplineTimer.start();
        EinsplineMultiEval (MultiSpline, ru, valVec, gradVec, StorageHessVector);
        EinsplineTimer.stop();
        for (int j=0; j<NumValenceOrbs; j++) {
          gradVec[j] = dot (PrimLattice.G, gradVec[j]);
// FIX FIX FIX: store transpose(PrimLattice.G)
          tmphs = dot(transpose(PrimLattice.G),StorageHessVector[j]);
          hessVec[j] = dot(tmphs,PrimLattice.G);
        }

        // Add e^-ikr phase to B-spline orbitals
        for (int j=0; j<NumValenceOrbs; j++) {
          complex<double> u = valVec[j];
          TinyVector<complex<double>,OHMMS_DIM> gradu = gradVec[j];
          tmphs = hessVec[j];
          PosType k = kPoints[j];
          TinyVector<complex<double>,OHMMS_DIM> ck;
          for (int n=0; n<OHMMS_DIM; n++)         ck[n] = k[n];
          double s,c;
          double phase = -dot(r, k);
          sincos (phase, &s, &c);
          complex<double> e_mikr (c,s);
          valVec[j]   = e_mikr*u;
          gradVec[j]  = e_mikr*(-eye*u*ck + gradu);
          hessVec[j]  = e_mikr*(tmphs -u*outerProduct(ck,ck) - eye*outerProduct(ck,gradu) - eye*outerProduct(gradu,ck)); 
        }
      }
    }

    // Finally, copy into output vectors
    int psiIndex = 0;
    int N = StorageValueVector.size();
    if (need2blend) {
      APP_ABORT("need2blend not implemented with Hessian evaluation.\n");
      for (int j=0; j<NumValenceOrbs; j++) {
        complex<double> psi_val, psi_lapl;
        TinyVector<complex<double>, OHMMS_DIM> psi_grad;
        PosType rhat = 1.0/std::sqrt(dot(disp,disp)) * disp;
        complex<double> psi1 = StorageValueVector[j];
        complex<double> psi2 =   BlendValueVector[j];
        TinyVector<complex<double>,OHMMS_DIM> dpsi1 = StorageGradVector[j];
        TinyVector<complex<double>,OHMMS_DIM> dpsi2 = BlendGradVector[j];
        complex<double> d2psi1 = StorageLaplVector[j];
        complex<double> d2psi2 =   BlendLaplVector[j];

        TinyVector<complex<double>,OHMMS_DIM> zrhat;
        for (int i=0; i<OHMMS_DIM; i++)
          zrhat[i] = rhat[i];

        psi_val  = b * psi1 + (1.0-b)*psi2;
        psi_grad = b * dpsi1 + (1.0-b)*dpsi2 + db * (psi1 - psi2)* zrhat;
        psi_lapl = b * d2psi1 + (1.0-b)*d2psi2 +
          2.0*db * (dot(zrhat,dpsi1) - dot(zrhat, dpsi2)) +
          d2b * (psi1 - psi2);

        psi[psiIndex] = real(psi_val);
        for (int n=0; n<OHMMS_DIM; n++)
          dpsi[psiIndex][n] = real(psi_grad[n]);
        //d2psi[psiIndex] = real(psi_lapl);
        psiIndex++;
        if (MakeTwoCopies[j]) {
          psi[psiIndex] = imag(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi[psiIndex][n] = imag(psi_grad[n]);
          //d2psi[psiIndex] = imag(psi_lapl);
          psiIndex++;
        }
      }
      for (int j=NumValenceOrbs; j<N; j++) {
        complex<double> psi_val, psi_lapl;
        TinyVector<complex<double>, OHMMS_DIM> psi_grad;
        psi_val  = StorageValueVector[j];
        psi_grad = StorageGradVector[j];
        psi_lapl = StorageLaplVector[j];

        psi[psiIndex] = real(psi_val);
        for (int n=0; n<OHMMS_DIM; n++)
          dpsi[psiIndex][n] = real(psi_grad[n]);
        //d2psi[psiIndex] = real(psi_lapl);
        psiIndex++;
        if (MakeTwoCopies[j]) {
          psi[psiIndex] = imag(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi[psiIndex][n] = imag(psi_grad[n]);
          //d2psi[psiIndex] = imag(psi_lapl);
          psiIndex++;
        }
      }
    }
    else {
      for (int j=0; j<N; j++) {
        complex<double> psi_val;
        TinyVector<complex<double>, OHMMS_DIM> psi_grad;
        psi_val  = StorageValueVector[j];
        psi_grad = StorageGradVector[j];
        tmphs = StorageHessVector[j];

        psi[psiIndex] = real(psi_val);
        for (int n=0; n<OHMMS_DIM; n++)
          dpsi[psiIndex][n] = real(psi_grad[n]);
        //d2psi[psiIndex] = real(psi_lapl);
// FIX FIX FIX
        for (int n=0; n<OHMMS_DIM*OHMMS_DIM; n++)
          grad_grad_psi[psiIndex][n] = real(tmphs(n));
        psiIndex++;
        if (MakeTwoCopies[j]) {
          psi[psiIndex] = imag(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi[psiIndex][n] = imag(psi_grad[n]);
          //d2psi[psiIndex] = imag(psi_lapl);
          for (int n=0; n<OHMMS_DIM*OHMMS_DIM; n++)
            grad_grad_psi[psiIndex][n] = imag(tmphs(n));
          psiIndex++;
        }
      }
    }
    VGLTimer.stop();
*/
  }

  
  // Value, gradient, and laplacian
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, ComplexValueVector_t& psi, 
   ComplexGradVector_t& dpsi, ComplexValueVector_t& d2psi)
  {
    VGLTimer.start();
    PosType r (P.R[iat]);
    PosType ru(PrimLattice.toUnit(P.R[iat]));
    for (int i=0; i<OHMMS_DIM; i++)
      ru[i] -= std::floor (ru[i]);

    EinsplineTimer.start();
    EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
			StorageGradVector, StorageHessVector);
    EinsplineTimer.stop();
    //computePhaseFactors(r);
    complex<double> eye (0.0, 1.0);
    for (int j=0; j<psi.size(); j++) {
      complex<double> u, laplu;
      TinyVector<complex<double>, OHMMS_DIM> gradu;
      u = StorageValueVector[j];
      gradu = dot(PrimLattice.G, StorageGradVector[j]);
      laplu = trace(StorageHessVector[j], GGt);
      
      PosType k = kPoints[j];
      TinyVector<complex<double>,OHMMS_DIM> ck;
      for (int n=0; n<OHMMS_DIM; n++)	
	ck[n] = k[n];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      convert(e_mikr * u, psi[j]);
      convert(e_mikr*(-eye*u*ck + gradu), dpsi[j]);
      //convertVec(e_mikr*(-eye*u*ck + gradu), dpsi[j]);
      convert(e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu), d2psi[j]);
    }
    VGLTimer.stop();
  }
  
  // Value, gradient, and laplacian
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate
  (const ParticleSet& P, int iat, ComplexValueVector_t& psi,
   ComplexGradVector_t& dpsi, ComplexHessVector_t& grad_grad_psi)
  {
    VGLTimer.start();
    PosType r (P.R[iat]);
    PosType ru(PrimLattice.toUnit(P.R[iat]));
    for (int i=0; i<OHMMS_DIM; i++)
      ru[i] -= std::floor (ru[i]);

    EinsplineTimer.start();
    EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
                        StorageGradVector, StorageHessVector);
    EinsplineTimer.stop();
    //computePhaseFactors(r);
    complex<double> eye (0.0, 1.0);
    for (int j=0; j<psi.size(); j++) {
      complex<double> u;
      TinyVector<complex<double>, OHMMS_DIM> gradu;
      Tensor<complex<double>,OHMMS_DIM> hs,tmphs;
      u = StorageValueVector[j];
      gradu = dot(PrimLattice.G, StorageGradVector[j]);
      //laplu = trace(StorageHessVector[j], GGt);
      tmphs = dot(transpose(PrimLattice.G),StorageHessVector[j]);
      hs = dot(tmphs,PrimLattice.G);

      PosType k = kPoints[j];
      TinyVector<complex<double>,OHMMS_DIM> ck;
      for (int n=0; n<OHMMS_DIM; n++)
        ck[n] = k[n];
      double s,c;
      double phase = -dot(r, k);
      sincos (phase, &s, &c);
      complex<double> e_mikr (c,s);
      convert(e_mikr * u, psi[j]);
      convert(e_mikr*(-eye*u*ck + gradu), dpsi[j]);
      //convertVec(e_mikr*(-eye*u*ck + gradu), dpsi[j]);
      //convert(e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu), d2psi[j]);
      convert(e_mikr*(hs -u*outerProduct(ck,ck) - eye*outerProduct(ck,gradu) - eye*outerProduct(gradu,ck)),grad_grad_psi(j));
    }
    VGLTimer.stop();
  }

  
  template<> void
  EinsplineSetExtended<double>::evaluate
  (const ParticleSet& P, int iat, RealValueVector_t& psi, 
   RealGradVector_t& dpsi, RealValueVector_t& d2psi)
  {
    VGLTimer.start();
    PosType r (P.R[iat]);

    bool inAtom = false;
    for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
      inAtom = AtomicOrbitals[jat].evaluate (r, psi, dpsi, d2psi);
      if (inAtom) 
	break;
    }
    if (!inAtom) {
      PosType ru(PrimLattice.toUnit(P.R[iat]));
      
      int sign=0;
      for (int i=0; i<OHMMS_DIM; i++) {
	RealType img = std::floor(ru[i]);
	ru[i] -= img;
	sign += HalfG[i] * (int)img;
      }
      
      EinsplineTimer.start();
      EinsplineMultiEval (MultiSpline, ru, psi, StorageGradVector, 
			  StorageHessVector);
      EinsplineTimer.stop();
    
      if (sign & 1) 
	for (int j=0; j<psi.size(); j++) {
	  psi[j] *= -1.0;
	  StorageGradVector[j] *= -1.0;
	  StorageHessVector[j] *= -1.0;
	}
      for (int i=0; i<psi.size(); i++) {
	dpsi[i]  = dot(PrimLattice.G, StorageGradVector[i]);
	d2psi[i] = trace(StorageHessVector[i], GGt);
      }
    }
    VGLTimer.stop();
  }
  
  
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose
  (const ParticleSet& P, int first, int last, RealValueMatrix_t& psi, 
   RealGradMatrix_t& dpsi, RealValueMatrix_t& d2psi)
  {
    complex<double> eye(0.0,1.0);
    VGLMatTimer.start();
    for (int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);
      
      // Do core states first
      int icore = NumValenceOrbs;
      for (int tin=0; tin<MuffinTins.size(); tin++) {
	MuffinTins[tin].evaluateCore(r, StorageValueVector, StorageGradVector,
				     StorageLaplVector, icore);
	icore += MuffinTins[tin].get_num_core();
      }
      
      // Add phase to core orbitals
      for (int j=NumValenceOrbs; j<StorageValueVector.size(); j++) {
	complex<double> u = StorageValueVector[j];
	TinyVector<complex<double>,OHMMS_DIM> gradu = StorageGradVector[j];
	complex<double> laplu = StorageLaplVector[j];
	PosType k = kPoints[j];
	TinyVector<complex<double>,OHMMS_DIM> ck;
	for (int n=0; n<OHMMS_DIM; n++)	  ck[n] = k[n];
	double s,c;
	double phase = -dot(r, k);
	sincos (phase, &s, &c);
	complex<double> e_mikr (c,s);
	StorageValueVector[j] = e_mikr*u;
	StorageGradVector[j]  = e_mikr*(-eye*u*ck + gradu);
	StorageLaplVector[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);
      }
      
      // Check if we are in the muffin tin;  if so, evaluate
      bool inTin = false, need2blend = false;
      PosType disp;
      double b, db, d2b;
      for (int tin=0; tin<MuffinTins.size(); tin++) {
	MuffinTins[tin].inside(r, inTin, need2blend);
	if (inTin) {
	  MuffinTins[tin].evaluate (r, StorageValueVector, 
				    StorageGradVector, StorageLaplVector);
	  if (need2blend) {
	    disp = MuffinTins[tin].disp(r);
	    double dr = std::sqrt(dot(disp, disp));
	    MuffinTins[tin].blend_func(dr, b, db, d2b);
	  }
	  break;
	}
      }
      bool inAtom = false;
      for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
	inAtom = AtomicOrbitals[jat].evaluate 
	  (r, StorageValueVector, StorageGradVector, StorageLaplVector);
	if (inAtom) break;
      }
      
      StorageValueVector_t &valVec =  
	need2blend ? BlendValueVector : StorageValueVector;
      StorageGradVector_t &gradVec =  
	need2blend ? BlendGradVector : StorageGradVector;
      StorageValueVector_t &laplVec =  
	need2blend ? BlendLaplVector : StorageLaplVector;
      
      // Otherwise, evaluate the B-splines
      if (!inTin || need2blend) {
	if (!inAtom) {
	  PosType ru(PrimLattice.toUnit(P.R[iat]));
	  for (int i=0; i<OHMMS_DIM; i++)
	    ru[i] -= std::floor (ru[i]);
	  EinsplineTimer.start();
	  EinsplineMultiEval (MultiSpline, ru, valVec, gradVec, StorageHessVector);
	  EinsplineTimer.stop();
	  for (int j=0; j<NumValenceOrbs; j++) {
	    gradVec[j] = dot (PrimLattice.G, gradVec[j]);
	    laplVec[j] = trace (StorageHessVector[j], GGt);
	  }
	  
	  // Add e^-ikr phase to B-spline orbitals
	  for (int j=0; j<NumValenceOrbs; j++) {
	    complex<double> u = valVec[j];
	    TinyVector<complex<double>,OHMMS_DIM> gradu = gradVec[j];
	    complex<double> laplu = laplVec[j];
	    PosType k = kPoints[j];
	    TinyVector<complex<double>,OHMMS_DIM> ck;
	    for (int n=0; n<OHMMS_DIM; n++)	  ck[n] = k[n];
	    double s,c;
	    double phase = -dot(r, k);
	    sincos (phase, &s, &c);
	    complex<double> e_mikr (c,s);
	    valVec[j]   = e_mikr*u;
	    gradVec[j]  = e_mikr*(-eye*u*ck + gradu);
	    laplVec[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);
	  }
	}
      }
      
      // Finally, copy into output vectors
      int psiIndex = 0;
      int N = StorageValueVector.size();
      if (need2blend) {
	for (int j=0; j<NumValenceOrbs; j++) {
	  complex<double> psi_val, psi_lapl;
	  TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	  PosType rhat = 1.0/std::sqrt(dot(disp,disp)) * disp;
	  complex<double> psi1 = StorageValueVector[j];
	  complex<double> psi2 =   BlendValueVector[j];
	  TinyVector<complex<double>,OHMMS_DIM> dpsi1 = StorageGradVector[j];
	  TinyVector<complex<double>,OHMMS_DIM> dpsi2 = BlendGradVector[j];
	  complex<double> d2psi1 = StorageLaplVector[j];
	  complex<double> d2psi2 =   BlendLaplVector[j];
	  
	  TinyVector<complex<double>,OHMMS_DIM> zrhat;
	  for (int n=0; n<OHMMS_DIM; n++)
	    zrhat[n] = rhat[n];
	  
	  psi_val  = b * psi1 + (1.0-b)*psi2;
	  psi_grad = b * dpsi1 + (1.0-b)*dpsi2 + db * (psi1 - psi2)* zrhat;
	  psi_lapl = b * d2psi1 + (1.0-b)*d2psi2 +
	    2.0*db * (dot(zrhat,dpsi1) - dot(zrhat, dpsi2)) +
	    d2b * (psi1 - psi2);
	  
	  psi(i,psiIndex) = real(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi(i,psiIndex)[n] = real(psi_grad[n]);
	  d2psi(i,psiIndex) = real(psi_lapl);
	  psiIndex++;
	  if (MakeTwoCopies[j]) {
	    psi(i,psiIndex) = imag(psi_val);
	    for (int n=0; n<OHMMS_DIM; n++)
	      dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
	    d2psi(i,psiIndex) = imag(psi_lapl);
	    psiIndex++;
	  }
	} 
	// Copy core states
	for (int j=NumValenceOrbs; j<N; j++) {
	  complex<double> psi_val, psi_lapl;
	  TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	  psi_val  = StorageValueVector[j];
	  psi_grad = StorageGradVector[j];
	  psi_lapl = StorageLaplVector[j];
	  
	  psi(i,psiIndex) = real(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi(i,psiIndex)[n] = real(psi_grad[n]);
	  d2psi(i,psiIndex) = real(psi_lapl);
	  psiIndex++;
	  if (MakeTwoCopies[j]) {
	    psi(i,psiIndex) = imag(psi_val);
	    for (int n=0; n<OHMMS_DIM; n++)
	      dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
	    d2psi(i,psiIndex) = imag(psi_lapl);
	    psiIndex++;
	  }
	}
      }
      else { // No blending needed
	for (int j=0; j<N; j++) {
	  complex<double> psi_val, psi_lapl;
	  TinyVector<complex<double>, OHMMS_DIM> psi_grad;
	  psi_val  = StorageValueVector[j];
	  psi_grad = StorageGradVector[j];
	  psi_lapl = StorageLaplVector[j];
	  
	  psi(i,psiIndex) = real(psi_val);
	  for (int n=0; n<OHMMS_DIM; n++)
	    dpsi(i,psiIndex)[n] = real(psi_grad[n]);
	  d2psi(i,psiIndex) = real(psi_lapl);
	  psiIndex++;
	  // if (psiIndex >= dpsi.cols()) {
	  //   cerr << "Error:  out of bounds writing in EinsplineSet::evalate.\n"
	  // 	 << "psiIndex = " << psiIndex << "  dpsi.cols() = " << dpsi.cols() << endl;
	  // }
	  if (MakeTwoCopies[j]) {
	    psi(i,psiIndex) = imag(psi_val);
	    for (int n=0; n<OHMMS_DIM; n++)
	      dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
	    d2psi(i,psiIndex) = imag(psi_lapl);
	    psiIndex++;
	  }
	}
      }
    }
    VGLMatTimer.stop();
  }

  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose
  (const ParticleSet& P, int first, int last, RealValueMatrix_t& psi,
   RealGradMatrix_t& dpsi, RealHessMatrix_t& grad_grad_psi)
   {
    complex<double> eye(0.0,1.0);
    VGLMatTimer.start();
    for (int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);

      // Do core states first
      int icore = NumValenceOrbs;
      for (int tin=0; tin<MuffinTins.size(); tin++) {
        APP_ABORT("MuffinTins not implemented with Hessian evaluation.\n");
        MuffinTins[tin].evaluateCore(r, StorageValueVector, StorageGradVector,
                                     StorageHessVector, icore);
        icore += MuffinTins[tin].get_num_core();
      }

      // Add phase to core orbitals
      for (int j=NumValenceOrbs; j<StorageValueVector.size(); j++) {
        complex<double> u = StorageValueVector[j];
        TinyVector<complex<double>,OHMMS_DIM> gradu = StorageGradVector[j];
        Tensor<complex<double>,OHMMS_DIM> hs = StorageHessVector[j];
        PosType k = kPoints[j];
        TinyVector<complex<double>,OHMMS_DIM> ck;
        for (int n=0; n<OHMMS_DIM; n++)   ck[n] = k[n];
        double s,c;
        double phase = -dot(r, k);
        sincos (phase, &s, &c);
        complex<double> e_mikr (c,s);
        StorageValueVector[j] = e_mikr*u;
        StorageGradVector[j]  = e_mikr*(-eye*u*ck + gradu);
        StorageHessVector[j]  = e_mikr*(hs -u*outerProduct(ck,ck) - eye*outerProduct(ck,gradu) - eye*outerProduct(gradu,ck));
      }
      // Check if we are in the muffin tin;  if so, evaluate
      bool inTin = false, need2blend = false;
      PosType disp;
      double b, db, d2b;
      for (int tin=0; tin<MuffinTins.size(); tin++) {
        APP_ABORT("MuffinTins not implemented with Hessian evaluation.\n");
        MuffinTins[tin].inside(r, inTin, need2blend);
        if (inTin) {
          MuffinTins[tin].evaluate (r, StorageValueVector,
                                    StorageGradVector, StorageHessVector);
          if (need2blend) {
            disp = MuffinTins[tin].disp(r);
            double dr = std::sqrt(dot(disp, disp));
            //MuffinTins[tin].blend_func(dr, b, db, d2b);
          }
          break;
        }
      }

      bool inAtom = false;
      for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
        inAtom = AtomicOrbitals[jat].evaluate
          (r, StorageValueVector, StorageGradVector, StorageHessVector);
        if (inAtom) break;
      }

      StorageValueVector_t &valVec =
        need2blend ? BlendValueVector : StorageValueVector;
      StorageGradVector_t &gradVec =
        need2blend ? BlendGradVector : StorageGradVector;
      StorageHessVector_t &hessVec =
        need2blend ? BlendHessVector : StorageHessVector;
      Tensor<complex<double>,OHMMS_DIM> tmphs;
      // Otherwise, evaluate the B-splines
      if (!inTin || need2blend) {
        if (!inAtom) {
          PosType ru(PrimLattice.toUnit(P.R[iat]));
          for (int i=0; i<OHMMS_DIM; i++)
            ru[i] -= std::floor (ru[i]);
          EinsplineTimer.start();
          EinsplineMultiEval (MultiSpline, ru, valVec, gradVec, StorageHessVector);
          EinsplineTimer.stop();
          for (int j=0; j<NumValenceOrbs; j++) {
            gradVec[j] = dot (PrimLattice.G, gradVec[j]);
// FIX FIX FIX: store transpose(PrimLattice.G)
            tmphs = dot(transpose(PrimLattice.G),StorageHessVector[j]);
            hessVec[j] = dot(tmphs,PrimLattice.G);
          }

          // Add e^-ikr phase to B-spline orbitals
          for (int j=0; j<NumValenceOrbs; j++) {
            complex<double> u = valVec[j];
            TinyVector<complex<double>,OHMMS_DIM> gradu = gradVec[j];
            tmphs = hessVec[j];
            PosType k = kPoints[j];
            TinyVector<complex<double>,OHMMS_DIM> ck;
            for (int n=0; n<OHMMS_DIM; n++)       ck[n] = k[n];
            double s,c;
            double phase = -dot(r, k);
            sincos (phase, &s, &c);
            complex<double> e_mikr (c,s);
            valVec[j]   = e_mikr*u;
            gradVec[j]  = e_mikr*(-eye*u*ck + gradu);
            hessVec[j]  = e_mikr*(tmphs -u*outerProduct(ck,ck) - eye*outerProduct(ck,gradu) - eye*outerProduct(gradu,ck));
          }
        }
      }
      // Finally, copy into output vectors
      int psiIndex = 0;
      int N = StorageValueVector.size();
      if (need2blend) {

        APP_ABORT("need2blend not implemented with Hessian evaluation.\n");

        for (int j=0; j<NumValenceOrbs; j++) {
          complex<double> psi_val, psi_lapl;
          TinyVector<complex<double>, OHMMS_DIM> psi_grad;
          PosType rhat = 1.0/std::sqrt(dot(disp,disp)) * disp;
          complex<double> psi1 = StorageValueVector[j];
          complex<double> psi2 =   BlendValueVector[j];
          TinyVector<complex<double>,OHMMS_DIM> dpsi1 = StorageGradVector[j];
          TinyVector<complex<double>,OHMMS_DIM> dpsi2 = BlendGradVector[j];
          complex<double> d2psi1 = StorageLaplVector[j];
          complex<double> d2psi2 =   BlendLaplVector[j];

          TinyVector<complex<double>,OHMMS_DIM> zrhat;
          for (int n=0; n<OHMMS_DIM; n++)
            zrhat[n] = rhat[n];

          psi_val  = b * psi1 + (1.0-b)*psi2;
          psi_grad = b * dpsi1 + (1.0-b)*dpsi2 + db * (psi1 - psi2)* zrhat;
          psi_lapl = b * d2psi1 + (1.0-b)*d2psi2 +
            2.0*db * (dot(zrhat,dpsi1) - dot(zrhat, dpsi2)) +
            d2b * (psi1 - psi2);

          psi(i,psiIndex) = real(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi(i,psiIndex)[n] = real(psi_grad[n]);
          //d2psi(i,psiIndex) = real(psi_lapl);
          psiIndex++;
          if (MakeTwoCopies[j]) {
            psi(i,psiIndex) = imag(psi_val);
            for (int n=0; n<OHMMS_DIM; n++)
              dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
            //d2psi(i,psiIndex) = imag(psi_lapl);
            psiIndex++;
          }
        }
        // Copy core states
        for (int j=NumValenceOrbs; j<N; j++) {
          complex<double> psi_val, psi_lapl;
          TinyVector<complex<double>, OHMMS_DIM> psi_grad;
          psi_val  = StorageValueVector[j];
          psi_grad = StorageGradVector[j];
          psi_lapl = StorageLaplVector[j];

          psi(i,psiIndex) = real(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi(i,psiIndex)[n] = real(psi_grad[n]);
          //d2psi(i,psiIndex) = real(psi_lapl);
          psiIndex++;
          if (MakeTwoCopies[j]) {
            psi(i,psiIndex) = imag(psi_val);
            for (int n=0; n<OHMMS_DIM; n++)
              dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
            //d2psi(i,psiIndex) = imag(psi_lapl);
            psiIndex++;
          }
        }
      }
      else { // No blending needed
        for (int j=0; j<N; j++) {
          complex<double> psi_val;
          TinyVector<complex<double>, OHMMS_DIM> psi_grad;
          psi_val  = StorageValueVector[j];
          psi_grad = StorageGradVector[j];
          tmphs = StorageHessVector[j];

          psi(i,psiIndex) = real(psi_val);
          for (int n=0; n<OHMMS_DIM; n++)
            dpsi(i,psiIndex)[n] = real(psi_grad[n]);
          //d2psi(i,psiIndex) = real(psi_lapl);
// FIX FIX FIX
          for (int n=0; n<OHMMS_DIM*OHMMS_DIM; n++)
            grad_grad_psi(i,psiIndex)[n] = real(tmphs(n));
          psiIndex++;
          // if (psiIndex >= dpsi.cols()) {
          //   cerr << "Error:  out of bounds writing in EinsplineSet::evalate.\n"
          //     << "psiIndex = " << psiIndex << "  dpsi.cols() = " << dpsi.cols() << endl;
          // }
          if (MakeTwoCopies[j]) {
            psi(i,psiIndex) = imag(psi_val);
            for (int n=0; n<OHMMS_DIM; n++)
              dpsi(i,psiIndex)[n] = imag(psi_grad[n]);
            //d2psi(i,psiIndex) = imag(psi_lapl);
            for (int n=0; n<OHMMS_DIM*OHMMS_DIM; n++)
              grad_grad_psi(i,psiIndex)[n] = imag(tmphs(n));
            psiIndex++;
          }
        }
      }
    }
    VGLMatTimer.stop();
   }
  
#if !defined(QMC_COMPLEX)
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluateGradSource
  (const ParticleSet& P, int first, int last,
   const ParticleSet &source, int iat, RealGradMatrix_t& dpsi)
  {
    complex<double> eye(0.0,1.0);
    
    // Loop over dimensions
    for (int dim=0; dim<OHMMS_DIM; dim++) {
      // Loop over electrons
      for(int iel=first,i=0; iel<last; iel++,i++) {
	PosType r (P.R[iel]);
	PosType ru(PrimLattice.toUnit(P.R[iel]));

	assert (FirstOrderSplines[iat][dim]);
	EinsplineMultiEval (FirstOrderSplines[iat][dim], ru, 
			    StorageValueVector);
	int dpsiIndex=0;
	for (int j=0; j<NumValenceOrbs; j++) {
	  PosType k = kPoints[j];
	  double s,c;
	  double phase = -dot(r, k);
	  sincos (phase, &s, &c);
	  complex<double> e_mikr (c,s);
	  StorageValueVector[j] *= e_mikr;
	  dpsi(i,dpsiIndex)[dim] = real(StorageValueVector[j]);
	  dpsiIndex++;
	  if (MakeTwoCopies[j]) {
	    dpsi(i,dpsiIndex)[dim] = imag(StorageValueVector[j]);
	    dpsiIndex++;
	  }
	}
      }
    }
    for (int i=0; i<(last-first); i++)
      for (int j=0; j<(last-first); j++) 
	dpsi(i,j) = dot (PrimLattice.G, dpsi(i,j));
     

  }


  // Evaluate the gradient w.r.t. to ion iat of the gradient and
  // laplacian of the orbitals w.r.t. the electrons
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluateGradSource 
  (const ParticleSet &P, int first, int last, 
   const ParticleSet &source, int iat_src,
   RealGradMatrix_t &dphi, RealHessMatrix_t &dgrad_phi, RealGradMatrix_t &dlapl_phi)
  {
    complex<double> eye(0.0,1.0);
    
    // Loop over dimensions
    for (int dim=0; dim<OHMMS_DIM; dim++) {
      // Loop over electrons
      for(int iel=first,i=0; iel<last; iel++,i++) {
	PosType r (P.R[iel]);
	PosType ru(PrimLattice.toUnit(P.R[iel]));

	assert (FirstOrderSplines[iat_src][dim]);
	EinsplineMultiEval (FirstOrderSplines[iat_src][dim], ru, 
			    StorageValueVector, StorageGradVector,
			    StorageHessVector);
	int dphiIndex=0;
	for (int j=0; j<NumValenceOrbs; j++) {
	  StorageGradVector[j] = dot (PrimLattice.G, StorageGradVector[j]);
	  StorageLaplVector[j] = trace (StorageHessVector[j], GGt);

	  complex<double> u = StorageValueVector[j];
	  TinyVector<complex<double>,OHMMS_DIM> gradu = StorageGradVector[j];
	  complex<double> laplu = StorageLaplVector[j];
	  PosType k = kPoints[j];
	  TinyVector<complex<double>,OHMMS_DIM> ck;
	  for (int n=0; n<OHMMS_DIM; n++)	  ck[n] = k[n];
	  double s,c;
	  double phase = -dot(r, k);
	  sincos (phase, &s, &c);
	  complex<double> e_mikr (c,s);
	  StorageValueVector[j]   = e_mikr*u;
	  StorageGradVector[j]  = e_mikr*(-eye*u*ck + gradu);
	  StorageLaplVector[j]  = e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu);

	  dphi(i,dphiIndex)[dim]    = real(StorageValueVector[j]);
	    for (int k=0; k<OHMMS_DIM; k++)
	      dgrad_phi(dphiIndex)[dim] = real(StorageGradVector[j][k]);
	  dlapl_phi(dphiIndex)[dim] = real(StorageLaplVector[j]);
	  dphiIndex++;
	  if (MakeTwoCopies[j]) {
	    dphi(i,dphiIndex)[dim] = imag(StorageValueVector[j]);
	    for (int k=0; k<OHMMS_DIM; k++)
	      dgrad_phi(i,dphiIndex)(dim,k) = imag(StorageGradVector[j][k]);
	    dlapl_phi(i,dphiIndex)[dim]     = imag(StorageLaplVector[j]);
	    dphiIndex++;
	  }
	}
      }
    }
    for (int i=0; i<(last-first); i++)
      for (int j=0; j<(last-first); j++) {
	dphi(i,j) = dot (PrimLattice.G, dphi(i,j));
	// Check this one!
	dgrad_phi(i,j) = dot (PrimLattice.G, dgrad_phi(i,j));
	dlapl_phi(i,j) = dot (PrimLattice.G, dlapl_phi(i,j));
      }
 

  }



  template<> void
  EinsplineSetExtended<double>::evaluateGradSource 
  (const ParticleSet &P, int first, int last, 
   const ParticleSet &source, int iat_src,
   RealGradMatrix_t &dphi, RealHessMatrix_t &dgrad_phi, 
   RealGradMatrix_t &dlapl_phi)
  {
    // Loop over dimensions
    for (int dim=0; dim<OHMMS_DIM; dim++) {
      assert (FirstOrderSplines[iat_src][dim]);
      // Loop over electrons
      for(int iel=first,i=0; iel<last; iel++,i++) {
	PosType r (P.R[iel]);
	PosType ru(PrimLattice.toUnit(r));
	int sign=0;
	for (int n=0; n<OHMMS_DIM; n++) {
	  RealType img = std::floor(ru[n]);
	  ru[n] -= img;
	  sign += HalfG[n] * (int)img;
	}
	for (int n=0; n<OHMMS_DIM; n++)
	  ru[n] -= std::floor (ru[n]);

	EinsplineMultiEval (FirstOrderSplines[iat_src][dim], ru, 
			    StorageValueVector, StorageGradVector,
			    StorageHessVector);
	if (sign & 1) 
	  for (int j=0; j<OrbitalSetSize; j++) {
	    dphi(i,j)[dim]      = -1.0 * StorageValueVector[j];
	    PosType g = -1.0*dot(PrimLattice.G, StorageGradVector[j]);
	    for (int k=0; k<OHMMS_DIM; k++)  dgrad_phi(i,j)(dim,k) = g[k];
	    dlapl_phi(i,j)[dim] = -1.0*trace (StorageHessVector[j], GGt);
	  }
	else 
	  for (int j=0; j<OrbitalSetSize; j++)  {
	    dphi(i,j)[dim] = StorageValueVector[j];
	    PosType g = dot(PrimLattice.G, StorageGradVector[j]);
	    for (int k=0; k<OHMMS_DIM; k++) dgrad_phi(i,j)(dim,k) = g[k];
	    dlapl_phi(i,j)[dim] = trace (StorageHessVector[j], GGt);
	  }
      }
    }
    for (int i=0; i<(last-first); i++)
      for (int j=0; j<(last-first); j++)  {
	dphi(i,j) = dot (PrimLattice.G, dphi(i,j));
	// Check this one!
	dgrad_phi(i,j) = dot (PrimLattice.G, dgrad_phi(i,j));
	dlapl_phi(i,j) = dot (PrimLattice.G, dlapl_phi(i,j));
      } 
  }
  template<> void
  EinsplineSetExtended<double>::evaluateGradSource
  (const ParticleSet& P, int first, int last,
   const ParticleSet &source, int iat, RealGradMatrix_t& dpsi)
  {
    // Loop over dimensions
    for (int dim=0; dim<OHMMS_DIM; dim++) {
      assert (FirstOrderSplines[iat][dim]);
      // Loop over electrons
      for(int iel=first,i=0; iel<last; iel++,i++) {
	PosType r (P.R[iel]);
	PosType ru(PrimLattice.toUnit(r));
	int sign=0;
	for (int n=0; n<OHMMS_DIM; n++) {
	  RealType img = std::floor(ru[n]);
	  ru[n] -= img;
	  sign += HalfG[n] * (int)img;
	}
	for (int n=0; n<OHMMS_DIM; n++)
	  ru[n] -= std::floor (ru[n]);
	
	EinsplineMultiEval (FirstOrderSplines[iat][dim], ru, 
			    StorageValueVector);
	if (sign & 1) 
	  for (int j=0; j<OrbitalSetSize; j++) 
	    dpsi(i,j)[dim] = -1.0 * StorageValueVector[j];
	else
	  for (int j=0; j<OrbitalSetSize; j++) 
	    dpsi(i,j)[dim] = StorageValueVector[j];
      }
    }
    for (int i=0; i<(last-first); i++)
      for (int j=0; j<(last-first); j++) {
	dpsi(i,j) = dot (PrimLattice.G, dpsi(i,j));
      }
    
  }
  
#endif

  
  
  
  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose
  (const ParticleSet& P, int first, int last, ComplexValueMatrix_t& psi, 
   ComplexGradMatrix_t& dpsi, ComplexValueMatrix_t& d2psi)
  {
    VGLMatTimer.start();
    for(int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);
      PosType ru(PrimLattice.toUnit(P.R[iat]));
      for (int n=0; n<OHMMS_DIM; n++)
	ru[n] -= std::floor (ru[n]);
      EinsplineTimer.start();
      EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
			  StorageGradVector, StorageHessVector);
      EinsplineTimer.stop();
      //computePhaseFactors(r);
      complex<double> eye (0.0, 1.0);
      for (int j=0; j<OrbitalSetSize; j++) {
	complex<double> u, laplu;
	TinyVector<complex<double>, OHMMS_DIM> gradu;
	u = StorageValueVector[j];
	gradu = dot(PrimLattice.G, StorageGradVector[j]);
	laplu = trace(StorageHessVector[j], GGt);
	
	PosType k = kPoints[j];
	TinyVector<complex<double>,OHMMS_DIM> ck;
	for (int n=0; n<OHMMS_DIM; n++)	
	  ck[n] = k[n];
	double s,c;
	double phase = -dot(r, k);
	sincos (phase, &s, &c);
	complex<double> e_mikr (c,s);
	convert(e_mikr * u, psi(i,j));
	//convert(e_mikr * u, psi(j,i));
	convert(e_mikr*(-eye*u*ck + gradu), dpsi(i,j));
	//convertVec(e_mikr*(-eye*u*ck + gradu), dpsi(i,j));
	convert(e_mikr*(-dot(k,k)*u - 2.0*eye*dot(ck,gradu) + laplu), d2psi(i,j));
      } 
    }
    VGLMatTimer.stop();
  }

   template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose
  (const ParticleSet& P, int first, int last, ComplexValueMatrix_t& psi,
   ComplexGradMatrix_t& dpsi, ComplexHessMatrix_t& grad_grad_psi)
  {
    VGLMatTimer.start();
    for(int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);
      PosType ru(PrimLattice.toUnit(P.R[iat]));
      for (int n=0; n<OHMMS_DIM; n++)
        ru[n] -= std::floor (ru[n]);
      EinsplineTimer.start();
      EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
                          StorageGradVector, StorageHessVector);
      EinsplineTimer.stop();
      //computePhaseFactors(r);
      complex<double> eye (0.0, 1.0);
      for (int j=0; j<OrbitalSetSize; j++) {
        complex<double> u;
        TinyVector<complex<double>, OHMMS_DIM> gradu;
        Tensor<complex<double>,OHMMS_DIM> hs,tmphs;
        u = StorageValueVector[j];
        gradu = dot(PrimLattice.G, StorageGradVector[j]);
        tmphs = dot(transpose(PrimLattice.G),StorageHessVector[j]);
        hs = dot(tmphs,PrimLattice.G);
        //laplu = trace(StorageHessVector[j], GGt);

        PosType k = kPoints[j];
        TinyVector<complex<double>,OHMMS_DIM> ck;
        for (int n=0; n<OHMMS_DIM; n++)
          ck[n] = k[n];
        double s,c;
        double phase = -dot(r, k);
        sincos (phase, &s, &c);
        complex<double> e_mikr (c,s);
        convert(e_mikr * u, psi(i,j));
        //convert(e_mikr * u, psi(j,i));
        convert(e_mikr*(-eye*u*ck + gradu), dpsi(i,j));
        //convertVec(e_mikr*(-eye*u*ck + gradu), dpsi(i,j));
        convert(e_mikr*(hs -u*outerProduct(ck,ck) - eye*outerProduct(ck,gradu) - eye*outerProduct(gradu,ck)),grad_grad_psi(i,j));
      }
    }
    VGLMatTimer.stop();
  }
  
  template<> void
    EinsplineSetExtended<double>::evaluate_notranspose(const ParticleSet& P
        , int first, int last, RealValueMatrix_t& psi
        , RealGradMatrix_t& dpsi, RealValueMatrix_t& d2psi)
  {
    VGLMatTimer.start();
    for(int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);

      bool inAtom = false;
      for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
	inAtom = AtomicOrbitals[jat].evaluate 
	  (r, StorageValueVector, StorageGradVector, StorageLaplVector);
	if (inAtom) {
	  for (int j=0; j<OrbitalSetSize; j++) {
	    psi(i,j)   = StorageValueVector[j];
	    dpsi(i,j)  = StorageGradVector[j];
	    d2psi(i,j) = StorageLaplVector[j];
	  }  
	  break;
	}
      }
      if (!inAtom) {
	PosType ru(PrimLattice.toUnit(P.R[iat]));
	int sign=0;
	for (int n=0; n<OHMMS_DIM; n++) {
	  RealType img = std::floor(ru[n]);
	  ru[n] -= img;
	  sign += HalfG[n] * (int)img;
	}
	for (int n=0; n<OHMMS_DIM; n++)
	  ru[n] -= std::floor (ru[n]);
	EinsplineTimer.start();
	EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
			    StorageGradVector, StorageHessVector);
	EinsplineTimer.stop();
	
	if (sign & 1) 
	  for (int j=0; j<OrbitalSetSize; j++) {
	    StorageValueVector[j] *= -1.0;
	    StorageGradVector[j]  *= -1.0;
	    StorageHessVector[j]  *= -1.0;
	  }
	for (int j=0; j<OrbitalSetSize; j++) {
	  psi(i,j)   = StorageValueVector[j];
	  dpsi(i,j)  = dot(PrimLattice.G, StorageGradVector[j]);
	  d2psi(i,j) = trace(StorageHessVector[j], GGt);
	}
      }
    }
    
    VGLMatTimer.stop();
  }
 
  template<> void
    EinsplineSetExtended<double>::evaluate_notranspose(const ParticleSet& P
        , int first, int last, RealValueMatrix_t& psi
        , RealGradMatrix_t& dpsi, RealHessMatrix_t& grad_grad_psi)
  {
    VGLMatTimer.start();
    for(int iat=first,i=0; iat<last; iat++,i++) {
      PosType r (P.R[iat]);

      bool inAtom = false;
      for (int jat=0; jat<AtomicOrbitals.size(); jat++) {
        inAtom = AtomicOrbitals[jat].evaluate
          (r, StorageValueVector, StorageGradVector, StorageHessVector);
        if (inAtom) {
          for (int j=0; j<OrbitalSetSize; j++) {
            psi(i,j)   = StorageValueVector[j];
            dpsi(i,j)  = StorageGradVector[j];
            grad_grad_psi(i,j) = StorageHessVector[j];
          }
          break;
        }
      }
      if (!inAtom) {
        PosType ru(PrimLattice.toUnit(P.R[iat]));
        int sign=0;
        for (int n=0; n<OHMMS_DIM; n++) {
          RealType img = std::floor(ru[n]);
        ru[n] -= img;
        sign += HalfG[n] * (int)img;
        }
        for (int n=0; n<OHMMS_DIM; n++)
          ru[n] -= std::floor (ru[n]);
        EinsplineTimer.start();
        EinsplineMultiEval (MultiSpline, ru, StorageValueVector,
                            StorageGradVector, StorageHessVector);
        EinsplineTimer.stop();
        if (sign & 1)
          for (int j=0; j<OrbitalSetSize; j++) {
            StorageValueVector[j] *= -1.0;
            StorageGradVector[j]  *= -1.0;
            StorageHessVector[j]  *= -1.0;
          }
        for (int j=0; j<OrbitalSetSize; j++) {
          psi(i,j)   = StorageValueVector[j];
          dpsi(i,j)  = dot(PrimLattice.G, StorageGradVector[j]);
          grad_grad_psi(i,j) = StorageHessVector[j];
        }
      }
    }
    VGLMatTimer.stop();
   }

   template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose(const ParticleSet& P, int first, int last,
                  RealValueMatrix_t& psi, RealGradMatrix_t& dpsi,
                  RealHessMatrix_t& grad_grad_psi,
                  RealGGGMatrix_t& grad_grad_grad_logdet)
    {
      APP_ABORT(" EinsplineSetExtended<StorageType>::evaluate_notranspose not implemented for grad_grad_grad_logdet yet. \n");
    }

   template<typename StorageType> void
  EinsplineSetExtended<StorageType>::evaluate_notranspose(const ParticleSet& P, int first, int last,
                  ComplexValueMatrix_t& psi, ComplexGradMatrix_t& dpsi,
                  ComplexHessMatrix_t& grad_grad_psi,
                  ComplexGGGMatrix_t& grad_grad_grad_logdet)
    {
      APP_ABORT(" EinsplineSetExtended<StorageType>::evaluate_notranspose not implemented for grad_grad_grad_logdet yet. \n");
    }

 
  template<typename StorageType> string
  EinsplineSetExtended<StorageType>::Type()
  {
    return "EinsplineSetExtended";
  }


  template<typename StorageType> void
  EinsplineSetExtended<StorageType>::registerTimers()
  {
    ValueTimer.reset();
    VGLTimer.reset();
    VGLMatTimer.reset();
    EinsplineTimer.reset();
    TimerManager.addTimer (&ValueTimer);
    TimerManager.addTimer (&VGLTimer);
    TimerManager.addTimer (&VGLMatTimer);
    TimerManager.addTimer (&EinsplineTimer);
  }



  template<typename StorageType> SPOSetBase*
  EinsplineSetExtended<StorageType>::makeClone() const
  {
    EinsplineSetExtended<StorageType> *clone = 
      new EinsplineSetExtended<StorageType> (*this);
    clone->registerTimers();
    for (int iat=0; iat<clone->AtomicOrbitals.size(); iat++)
      clone->AtomicOrbitals[iat].registerTimers();
    return clone;
  }

  template class EinsplineSetExtended<complex<double> >;
  template class EinsplineSetExtended<        double  >;


#ifdef QMC_CUDA
  ///////////////////////////////
  // Real StorageType versions //
  ///////////////////////////////
  template<> string
  EinsplineSetHybrid<double>::Type()
  {
  }
  
  
  template<typename StorageType> SPOSetBase*
  EinsplineSetHybrid<StorageType>::makeClone() const
  {
    EinsplineSetHybrid<StorageType> *clone = 
      new EinsplineSetHybrid<StorageType> (*this);
    clone->registerTimers();
    return clone;
  }
  

  //////////////////////////////////
  // Complex StorageType versions //
  //////////////////////////////////

  
  template<> string
  EinsplineSetHybrid<complex<double> >::Type()
  {
  }
  






  template<>
  EinsplineSetHybrid<double>::EinsplineSetHybrid() :
    CurrentWalkers(0)
  {
    ValueTimer.set_name ("EinsplineSetHybrid::ValueOnly");
    VGLTimer.set_name ("EinsplineSetHybrid::VGL");
    ValueTimer.set_name ("EinsplineSetHybrid::VGLMat");
    EinsplineTimer.set_name ("EinsplineSetHybrid::Einspline");
    className = "EinsplineSeHybrid";
    TimerManager.addTimer (&ValueTimer);
    TimerManager.addTimer (&VGLTimer);
    TimerManager.addTimer (&VGLMatTimer);
    TimerManager.addTimer (&EinsplineTimer);
    for (int i=0; i<OHMMS_DIM; i++)
      HalfG[i] = 0;
  }

  template<>
  EinsplineSetHybrid<complex<double > >::EinsplineSetHybrid() :
    CurrentWalkers(0)
  {
    ValueTimer.set_name ("EinsplineSetHybrid::ValueOnly");
    VGLTimer.set_name ("EinsplineSetHybrid::VGL");
    ValueTimer.set_name ("EinsplineSetHybrid::VGLMat");
    EinsplineTimer.set_name ("EinsplineSetHybrid::Einspline");
    className = "EinsplineSeHybrid";
    TimerManager.addTimer (&ValueTimer);
    TimerManager.addTimer (&VGLTimer);
    TimerManager.addTimer (&VGLMatTimer);
    TimerManager.addTimer (&EinsplineTimer);
    for (int i=0; i<OHMMS_DIM; i++)
      HalfG[i] = 0;
  }

  template class EinsplineSetHybrid<complex<double> >;
  template class EinsplineSetHybrid<        double  >;
#endif
}
