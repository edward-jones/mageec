//===-- FeatureExtract - MAGEEC Feature Extraction pass -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass performs feature extraction on LLVM IR, to be used with
// the MAGEEC framework.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "mageec/Database.h"
#include "mageec/Framework.h"

#include "Features.h"

using namespace llvm;

#define DEBUG_TYPE "feature-extract"


//static cl::opt<std::string>
//    FeaturesFilename("export-features", cl::value_desc("filename"),
//                     cl::desc("A file to export feature identifiers to"),
//                     cl::Hidden);
static cl::opt<std::string>
    DatabaseFilename("mageec-database", cl::value_desc("filename"),
                     cl::desc("The MAGEEC database to insert features into"),
                     cl::Hidden);


namespace {
class FeatureExtract : public ModulePass {
public:
  static char ID;
  FeatureExtract() : ModulePass(ID) {}

  StringRef getPassName() const override {
    return "MAGEEC Feature Extraction";
  }

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

typedef std::map<unsigned, unsigned> FunctionFeatures;
typedef std::map<unsigned, unsigned> ModuleFeatures;

} // end of anonymous namespace

char FeatureExtract::ID = 0;
static RegisterPass<FeatureExtract> X("feature-extract",
                                      "MAGEEC Feature Extraction",
                                      false, false);

// Hooks to add the feature extract as a standard pass when loaded into
// clang.
// loadPass is of type PassManagerBuilder::ExtensionFn
static void loadPass(const PassManagerBuilder &Builder, PassManagerBase &PM) {
  PM.add(new FeatureExtract());
}

// Static constructors which add the pass to the list of global extensions
// This would normally add the pass twice, however EP_EnabledOnOptLevel0 only
// runs at -O0, and EP_ModuleOptimizerEarly only runs at anything but -O0
static RegisterStandardPasses FeatureExtractLoader_O0(
    PassManagerBuilder::EP_EnabledOnOptLevel0, loadPass);
static RegisterStandardPasses FeatureExtractLoader_Ox(
    PassManagerBuilder::EP_ModuleOptimizerEarly, loadPass);


static FunctionFeatures extractFunctionFeatures(Function &F) {
  FunctionFeatures Features;
  Features[FunctionFeature::kInstrCount]           = 5;
  Features[FunctionFeature::kBBCount]              = 6;
  Features[FunctionFeature::kCFGEdges]             = 26;
  Features[FunctionFeature::kCyclomaticComplexity] = 42;
  Features[FunctionFeature::kCriticalPathLen]      = 88;
  return Features;
}


static ModuleFeatures
extractModuleFeatures(std::map<StringRef, FunctionFeatures> &M) {
  ModuleFeatures Features;

  Features[ModuleFeature::kFuncCount] = 1;

  Features[ModuleFeature::kFuncInstrCountRange]  = 25;
  Features[ModuleFeature::kFuncInstrCountMean]   = 56;
  Features[ModuleFeature::kFuncInstrCountMedian] = 22;

  Features[ModuleFeature::kFuncBBCountRange]     = 72;
  Features[ModuleFeature::kFuncBBCountMean]      = 1;
  Features[ModuleFeature::kFuncBBCountMedian]    = 13;

  Features[ModuleFeature::kFuncCFGEdgesRange]    = 144;
  Features[ModuleFeature::kFuncCFGEdgesMean]     = 22;
  Features[ModuleFeature::kFuncCFGEdgesMedian]   = 16;

  Features[ModuleFeature::kFuncCyclomaticComplexityRange]  = 11;
  Features[ModuleFeature::kFuncCyclomaticComplexityMean]   = 5;
  Features[ModuleFeature::kFuncCyclomaticComplexityMedian] = 181;

  Features[ModuleFeature::kFuncCriticalPathLenRange]  = 22;
  Features[ModuleFeature::kFuncCriticalPathLenMean]   = 23;
  Features[ModuleFeature::kFuncCriticalPathLenMedian] = 44;

  return Features;
}


bool FeatureExtract::runOnModule(Module &M) {
  //if (FeaturesFilename.empty()) {
  //  llvm::report_fatal_error("mageec feature extractor requires a file to "
  //                           "export features to", false);
  //}
  if (DatabaseFilename.empty()) {
    DatabaseFilename = "hello.db";
    //llvm::report_fatal_error("mageec feature extractor requires a database to "
    //                         "save features to", false);
  }

  mageec::Framework Framework;
  auto Database = Framework.getDatabase(DatabaseFilename, false);
  if (!Database) {
    llvm::report_fatal_error("mageec feature extractor could not load "
                             "the provided database: Check the database "
                             "exists and you have sufficient permissions to "
                             "read/write it");
  }

  //std::error_code EC;
  //raw_fd_ostream FeaturesFile(FeaturesFilename, EC, sys::fs::OpenFlags::F_None);
  //ExitOnErr(errorCodeToError(EC));

  std::map<StringRef, FunctionFeatures> FuncFeatures;
  for (auto &F : M)
    FuncFeatures[F.getName()] = extractFunctionFeatures(F);
  ModuleFeatures ModFeatures = extractModuleFeatures(FuncFeatures);

	LLVMContext &C = M.getContext();
  for (auto &F : M) {
    mageec::FeatureSet FeatureSet;
    for (auto Entry : FuncFeatures[F.getName()]) {
      auto Feat = std::make_shared<mageec::IntFeature>(Entry.first,
                                                       Entry.second, "");
      FeatureSet.add(Feat);
    }
  	// Add the features to the database, get an identifier for those features
  	// and emit the identifier into the metadata for the function
    auto FeatSetID = Database->newFeatureSet(FeatureSet);
		MDNode *N =
        MDNode::get(C, MDString::get(C, std::to_string(static_cast<uint64_t>(FeatSetID))));
		F.setMetadata("mageec.feature.set", N);
    //FeaturesFile << FullPath
    //             << ",function," << F.getName()
    //             << ",features," << FeatSetID
    //             << ",feature_class,1\n";
  }

  mageec::FeatureSet FeatureSet;
  for (auto Entry : ModFeatures) {
    auto Feat = std::make_shared<mageec::IntFeature>(Entry.first,
                                                     Entry.second, "");
    FeatureSet.add(Feat);
  }
  // Add the features to the database, get an identifier for those features
  // and emit the identifier into the metadata for the module
  auto FeatSetID = Database->newFeatureSet(FeatureSet);
  //FeaturesFile << FullPath
  //             << ",module," << M.getName()
  //             << ",features," << FeatSetID
  //             << ",feature_class,0\n";

  NamedMDNode *NamedMD = M.getOrInsertNamedMetadata("mageec.feature.set");
	MDNode *N = MDNode::get(C, MDString::get(C, std::to_string(static_cast<uint64_t>(FeatSetID))));
  NamedMD->addOperand(N);
  return false;
}
