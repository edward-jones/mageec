//===-- Features - MAGEEC Features ----------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This defines the various features which are extracted by the LLVM
// feature extractor.
//
//===----------------------------------------------------------------------===//

#ifndef MAGEEC_LLVM_FEATURES_H
#define MAGEEC_LLVM_FEATURES_H

enum {
  kFunctionFeaturesBegin = 0x1000,
  kModuleFeaturesBegin   = 0x2000,
};

namespace FunctionFeature {
enum : unsigned {
  kInstrCount = kFunctionFeaturesBegin,
  kBBCount,
  kCFGEdges,
  kCyclomaticComplexity,
  kCriticalPathLen,
};
} // end of namespace FunctionFeature

namespace ModuleFeature {
enum : unsigned {
  kFuncCount = kModuleFeaturesBegin,

  kFuncInstrCountRange,
  kFuncInstrCountMean,
  kFuncInstrCountMedian,

  kFuncBBCountRange,
  kFuncBBCountMean,
  kFuncBBCountMedian,

  kFuncCFGEdgesRange,
  kFuncCFGEdgesMean,
  kFuncCFGEdgesMedian,

  kFuncCyclomaticComplexityRange,
  kFuncCyclomaticComplexityMean,
  kFuncCyclomaticComplexityMedian,

  kFuncCriticalPathLenRange,
  kFuncCriticalPathLenMean,
  kFuncCriticalPathLenMedian,
};
} // end of namespace ModuleFeature

#endif // MAGEEC_LLVM_FEATURES_H
