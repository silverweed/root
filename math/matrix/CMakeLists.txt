# Copyright (C) 1995-2019, Rene Brun and Fons Rademakers.
# All rights reserved.
#
# For the licensing terms see $ROOTSYS/LICENSE.
# For the list of contributors see $ROOTSYS/README/CREDITS.

############################################################################
# CMakeLists.txt file for building ROOT math/matrix package
############################################################################

ROOT_STANDARD_LIBRARY_PACKAGE(Matrix
  HEADERS
    TDecompBK.h
    TDecompBase.h
    TDecompChol.h
    TDecompLU.h
    TDecompQRH.h
    TDecompSVD.h
    TDecompSparse.h
    TMatrix.h
    TMatrixD.h
    TMatrixDBase.h
    TMatrixDBasefwd.h
    TMatrixDEigen.h
    TMatrixDLazy.h
    TMatrixDSparse.h
    TMatrixDSparsefwd.h
    TMatrixDSym.h
    TMatrixDSymEigen.h
    TMatrixDSymfwd.h
    TMatrixDUtils.h
    TMatrixDUtilsfwd.h
    TMatrixDfwd.h
    TMatrixF.h
    TMatrixFBase.h
    TMatrixFBasefwd.h
    TMatrixFLazy.h
    TMatrixFSparse.h
    TMatrixFSparsefwd.h
    TMatrixFSym.h
    TMatrixFSymfwd.h
    TMatrixFUtils.h
    TMatrixFUtilsfwd.h
    TMatrixFfwd.h
    TMatrixT.h
    TMatrixTBase.h
    TMatrixTCramerInv.h
    TMatrixTLazy.h
    TMatrixTSparse.h
    TMatrixTSym.h
    TMatrixTSymCramerInv.h
    TMatrixTUtils.h
    TVector.h
    TVectorD.h
    TVectorDfwd.h
    TVectorF.h
    TVectorFfwd.h
    TVectorT.h
    TVectorfwd.h
  SOURCES
    src/TDecompBK.cxx
    src/TDecompBase.cxx
    src/TDecompChol.cxx
    src/TDecompLU.cxx
    src/TDecompQRH.cxx
    src/TDecompSVD.cxx
    src/TDecompSparse.cxx
    src/TMatrixDEigen.cxx
    src/TMatrixDSymEigen.cxx
    src/TMatrixT.cxx
    src/TMatrixTBase.cxx
    src/TMatrixTCramerInv.cxx
    src/TMatrixTLazy.cxx
    src/TMatrixTSparse.cxx
    src/TMatrixTSym.cxx
    src/TMatrixTSymCramerInv.cxx
    src/TMatrixTUtils.cxx
    src/TVectorT.cxx
 DEPENDENCIES
   MathCore
 DICTIONARY_OPTIONS
   -writeEmptyRootPCM
)

ROOT_ADD_TEST_SUBDIRECTORY(test)

