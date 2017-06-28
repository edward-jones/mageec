#include "mageec/Attribute.h"
#include "mageec/Database.h"
#include "mageec/Framework.h"
#include "mageec/ML/C5.h"
#include "mageec/ML/1NN.h"
#include "mageec/Util.h"
#include "Parameters.h"

#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <vector>

#if !defined(GCC_DRIVER_VERSION_MAJOR) ||                                      \
    !defined(GCC_DRIVER_VERSION_MINOR) ||                                      \
    !defined(GCC_DRIVER_VERSION_PATCH)
#error "MAGEEC gcc driver version not defined by the build system"
#endif
static_assert(GCC_DRIVER_VERSION_MAJOR == 1 && GCC_DRIVER_VERSION_MINOR == 0 &&
                  GCC_DRIVER_VERSION_PATCH == 0,
              "MAGEEC gcc driver version does not match");
static const mageec::util::Version gcc_driver_version(GCC_DRIVER_VERSION_MAJOR,
                                                      GCC_DRIVER_VERSION_MINOR,
                                                      GCC_DRIVER_VERSION_PATCH);

/// \enum DriverMode
///
/// \brief Modes which the GCC wrapper driver can run in
enum class DriverMode {
  /// Only utility methods can be accessed
  kNone,
  /// Gather mode, record the flags used in the compilation to a database
  kGather,
  /// Predict mode, replace the flags with flags derived by sending queries
  /// about flags to the machine learners
  kPredict
};


/// Map from a flag to an integer parameter to be used with MAGEEC, as well
/// as the gcc version that the flag is first supported.
// Heap allocate so the destructor does not need to be called
static const auto &all_flag_to_parameter = *new std::map<std::string, std::pair<unsigned, unsigned>>({
  {"-faggressive-loop-optimizations",      {FlagParameterID::kAggressiveLoopOptimizations, 40800}},
  {"-falign-functions",                    {FlagParameterID::kAlignFunctions, 40500}},
  {"-falign-jumps",                        {FlagParameterID::kAlignJumps, 40500}},
  {"-falign-labels",                       {FlagParameterID::kAlignLabels, 40500}},
  {"-falign-loops",                        {FlagParameterID::kAlignLoops, 40500}},
  {"-fbranch-count-reg",                   {FlagParameterID::kBranchCountReg, 40500}},
  {"-fbranch-target-load-optimize",        {FlagParameterID::kBranchTargetLoadOptimize, 40500}},
  // Can't run multiple times
  //{"-fbranch-target-load-optimize2",       {FlagParameterID::kBranchTargetLoadOptimize2, 40500}},
  {"-fbtr-bb-exclusive",                   {FlagParameterID::kBTRBBExclusive, 40500}},
  {"-fcaller-saves",                       {FlagParameterID::kCallerSaves, 40500}},
  {"-fcombine-stack-adjustments",          {FlagParameterID::kCombineStackAdjustments, 40600}},
  // affects semantics, unlikely to affect performance
  //{"-fcommon",                             {FlagParameterID::kCommon, 40500}},
  {"-fcompare-elim",                       {FlagParameterID::kCompareElim, 40600}},
  {"-fconserve-stack",                     {FlagParameterID::kConserveStack, 40500}},
  {"-fcprop-registers",                    {FlagParameterID::kCPropRegister, 40500}},
  {"-fcrossjumping",                       {FlagParameterID::kCrossJumping, 40500}},
  {"-fcse-follow-jumps",                   {FlagParameterID::kCSEFollowJumps, 40500}},
  // affects semantics, unlikely to affect performance
  //{"-fdata-sections",                      {FlagParameterID::kDataSections, 40500}},
  {"-fdce",                                {FlagParameterID::kDCE, 40500}},
  {"-fdefer-pop",                          {FlagParameterID::kDeferPop, 40500}},
  {"-fdelete-null-pointer-checks",         {FlagParameterID::kDeleteNullPointerChecks, 40500}},
  {"-fdevirtualize",                       {FlagParameterID::kDevirtualize, 40600}},
  {"-fdse",                                {FlagParameterID::kDSE, 40500}},
  {"-fearly-inlining",                     {FlagParameterID::kEarlyInlining, 40500}},
  {"-fexpensive-optimizations",            {FlagParameterID::kExpensiveOptimizations, 40500}},
  {"-fforward-propagate",                  {FlagParameterID::kForwardPropagate, 40500}},
  {"-fgcse",                               {FlagParameterID::kGCSE, 40500}},
  {"-fgcse-after-reload",                  {FlagParameterID::kGCSEAfterReload, 40500}},
  {"-fgcse-las",                           {FlagParameterID::kGCSELAS, 40500}},
  {"-fgcse-lm",                            {FlagParameterID::kGCSELM, 40500}},
  {"-fgcse-sm",                            {FlagParameterID::kGCSESM, 40500}},
  {"-fguess-branch-probability",           {FlagParameterID::kGuessBranchProbability, 40500}},
  {"-fhoist-adjacent-loads",               {FlagParameterID::kHoistAdjacentLoads, 40800}},
  {"-fif-conversion",                      {FlagParameterID::kIfConversion, 40500}},
  {"-fif-conversion2",                     {FlagParameterID::kIfConversion2, 40500}},
  {"-finline",                             {FlagParameterID::kInline, 40500}},
  {"-finline-atomics",                     {FlagParameterID::kInlineAtomics, 40700}},
  {"-finline-functions",                   {FlagParameterID::kInlineFunctions, 40500}},
  {"-finline-functions-called-once",       {FlagParameterID::kInlineFunctionsCalledOnce, 40500}},
  {"-finline-small-functions",             {FlagParameterID::kInlineSmallFunctions, 40500}},
  {"-fipa-cp",                             {FlagParameterID::kIPACP, 40500}},
  {"-fipa-cp-clone",                       {FlagParameterID::kIPACPClone, 40500}},
  {"-fipa-profile",                        {FlagParameterID::kIPAProfile, 40600}},
  {"-fipa-pta",                            {FlagParameterID::kIPAPTA, 40500}},
  {"-fipa-pure-const",                     {FlagParameterID::kIPAPureConst, 40500}},
  {"-fipa-reference",                      {FlagParameterID::kIPAReference, 40500}},
  {"-fipa-sra",                            {FlagParameterID::kIPASRA, 40500}},
  {"-fira-hoist-pressure",                 {FlagParameterID::kIRAHoistPressure, 40800}},
  {"-fivopts",                             {FlagParameterID::kIVOpts, 40500}},
  {"-fmerge-constants",                    {FlagParameterID::kMergeConstants, 40500}},
  {"-fmodulo-sched",                       {FlagParameterID::kModuloSched, 40500}},
  {"-fmove-loop-invariants",               {FlagParameterID::kMoveLoopInvariants, 40500}},
  {"-fomit-frame-pointer",                 {FlagParameterID::kOmitFramePointer, 40500}},
  {"-foptimize-sibling-calls",             {FlagParameterID::kOptimizeSiblingCalls, 40500}},
  {"-foptimize-strlen",                    {FlagParameterID::kOptimizeStrLen, 40700}},
  {"-fpeephole",                           {FlagParameterID::kPeephole, 40500}},
  {"-fpeephole2",                          {FlagParameterID::kPeephole2, 40500}},
  {"-fpredictive-commoning",               {FlagParameterID::kPredictiveCommoning, 40500}},
  {"-fprefetch-loop-arrays",               {FlagParameterID::kPrefetchLoopArrays, 40500}},
  {"-fregmove",                            {FlagParameterID::kRegMove, 40500}},
  {"-frename-registers",                   {FlagParameterID::kRenameRegisters, 40500}},
  {"-freorder-blocks",                     {FlagParameterID::kReorderBlocks, 40500}},
  {"-freorder-functions",                  {FlagParameterID::kReorderFunctions, 40500}},
  {"-frerun-cse-after-loop",               {FlagParameterID::kRerunCSEAfterLoop, 40500}},
  {"-freschedule-modulo-scheduled-loops",  {FlagParameterID::kRescheduleModuloScheduledLoops, 40500}},
  {"-fsched-critical-path-heuristic",      {FlagParameterID::kSchedCriticalPathHeuristic, 40500}},
  {"-fsched-dep-count-heuristic",          {FlagParameterID::kSchedDepCountHeuristic, 40500}},
  {"-fsched-group-heuristic",              {FlagParameterID::kSchedGroupHeuristic, 40500}},
  {"-fsched-interblock",                   {FlagParameterID::kSchedInterblock, 40500}},
  {"-fsched-last-insn-heuristic",          {FlagParameterID::kSchedLastInsnHeuristic, 40500}},
  {"-fsched-pressure",                     {FlagParameterID::kSchedPressure, 40500}},
  {"-fsched-rank-heuristic",               {FlagParameterID::kSchedRankHeuristic, 40500}},
  {"-fsched-spec",                         {FlagParameterID::kSchedSpec, 40500}},
  {"-fsched-spec-insn-heuristic",          {FlagParameterID::kSchedSpecInsnHeuristic, 40500}},
  {"-fsched-spec-load",                    {FlagParameterID::kSchedSpecLoad, 40500}},
  {"-fsched-stalled-insns",                {FlagParameterID::kSchedStalledInsns, 40500}},
  {"-fsched-stalled-insns-dep",            {FlagParameterID::kSchedStalledInsnsDep, 40500}},
  {"-fschedule-insns",                     {FlagParameterID::kScheduleInsns, 40500}},
  {"-fschedule-insns2",                    {FlagParameterID::kScheduleInsns2, 40500}},
  // may conflict with other flags
  //{"-fsection-anchors",                    {FlagParameterID::kSectionAnchors, 40500}},
  {"-fsel-sched-pipelining",               {FlagParameterID::kSelSchedPipelining, 40500}},
  {"-fsel-sched-pipelining-outer-loops",   {FlagParameterID::kSelSchedPipeliningOuterLoops, 40500}},
  {"-fsel-sched-reschedule-pipelined",     {FlagParameterID::kSelSchedReschedulePipelined, 40500}},
  {"-fselective-scheduling",               {FlagParameterID::kSelectiveScheduling, 40500}},
  {"-fselective-scheduling2",              {FlagParameterID::kSelectiveScheduling2, 40500}},
  {"-fshrink-wrap",                        {FlagParameterID::kShrinkWrap, 40700}},
  {"-fsplit-ivs-in-unroller",              {FlagParameterID::kSplitIVsInUnroller, 40500}},
  {"-fsplit-wide-types",                   {FlagParameterID::kSplitWideTypes, 40500}},
  // affects semantics
  //{"-fstrict-aliasing",                    {FlagParameterID::kStrictAliasing, 40500}},
  {"-fthread-jumps",                       {FlagParameterID::kThreadJumps, 40500}},
  {"-ftoplevel-reorder",                   {FlagParameterID::kTopLevelReorder, 40500}},
  {"-ftree-bit-ccp",                       {FlagParameterID::kTreeBitCCP, 40600}},
  {"-ftree-builtin-call-dce",              {FlagParameterID::kTreeBuiltinCallDCE, 40500}},
  {"-ftree-ccp",                           {FlagParameterID::kTreeCCP, 40500}},
  {"-ftree-ch",                            {FlagParameterID::kTreeCH, 40500}},
  // no corresponding -fno- for this flag
  //{"-ftree-coalesce-inlined-vars",         {FlagParameterID::kTreeCoalesceInlinedVars, 40500}},
  {"-ftree-coalesce-vars",                 {FlagParameterID::kTreeCoalesceVars, 40800}},
  {"-ftree-copy-prop",                     {FlagParameterID::kTreeCopyProp, 40500}},
  {"-ftree-copyrename",                    {FlagParameterID::kTreeCopyRename, 40500}},
  {"-ftree-cselim",                        {FlagParameterID::kTreeCSEElim, 40500}},
  {"-ftree-dce",                           {FlagParameterID::kTreeDCE, 40500}},
  {"-ftree-dominator-opts",                {FlagParameterID::kTreeDominatorOpts, 40500}},
  {"-ftree-dse",                           {FlagParameterID::kTreeDSE, 40500}},
  {"-ftree-forwprop",                      {FlagParameterID::kTreeForwProp, 40500}},
  {"-ftree-fre",                           {FlagParameterID::kTreeFRE, 40500}},
  {"-ftree-loop-distribute-patterns",      {FlagParameterID::kTreeLoopDistributePatterns, 40600}},
  {"-ftree-loop-distribution",             {FlagParameterID::kTreeLoopDistribution, 40500}},
  {"-ftree-loop-if-convert",               {FlagParameterID::kTreeLoopIfConvert, 40600}},
  {"-ftree-loop-im",                       {FlagParameterID::kTreeLoopIM, 40500}},
  {"-ftree-loop-ivcanon",                  {FlagParameterID::kTreeLoopIVCanon, 40500}},
  {"-ftree-loop-optimize",                 {FlagParameterID::kTreeLoopOptimize, 40500}},
  {"-ftree-partial-pre",                   {FlagParameterID::kTreePartialPre, 40800}},
  {"-ftree-phiprop",                       {FlagParameterID::kTreePhiProp, 40500}},
  {"-ftree-pre",                           {FlagParameterID::kTreePre, 40500}},
  {"-ftree-pta",                           {FlagParameterID::kTreePTA, 40500}},
  {"-ftree-reassoc",                       {FlagParameterID::kTreeReassoc, 40500}},
  {"-ftree-scev-cprop",                    {FlagParameterID::kTreeSCEVCProp, 40500}},
  {"-ftree-sink",                          {FlagParameterID::kTreeSink, 40500}},
  {"-ftree-slp-vectorize",                 {FlagParameterID::kTreeSLPVectorize, 40500}},
  {"-ftree-slsr",                          {FlagParameterID::kTreeSLSR, 40800}},
  {"-ftree-sra",                           {FlagParameterID::kTreeSRA, 40500}},
  {"-ftree-switch-conversion",             {FlagParameterID::kTreeSwitchConversion, 40500}},
  {"-ftree-tail-merge",                    {FlagParameterID::kTreeTailMerge, 40700}},
  {"-ftree-ter",                           {FlagParameterID::kTreeTER, 40500}},
  {"-ftree-vect-loop-version",             {FlagParameterID::kTreeVectLoopVersion, 40500}},
  {"-ftree-vectorize",                     {FlagParameterID::kTreeVectorize, 40500}},
  {"-ftree-vrp",                           {FlagParameterID::kTreeVRP, 40500}},
  {"-funroll-all-loops",                   {FlagParameterID::kUnrollAllLoops, 40500}},
  {"-funroll-loops",                       {FlagParameterID::kUnrollLoops, 40500}},
  {"-funswitch-loops",                     {FlagParameterID::kUnswitchLoops, 40500}},
  {"-fvariable-expansion-in-unroller",     {FlagParameterID::kVariableExpansionInUnroller, 40500}},
  {"-fvect-cost-model",                    {FlagParameterID::kVectCostModel, 40500}},
  {"-fweb",                                {FlagParameterID::kWeb, 40500}},
});

struct ParameterToFlag {
  ParameterToFlag() {}
  operator std::map<unsigned, std::pair<std::string, unsigned>>() {
    static bool is_init = false;
    static auto &all_parameter_to_flag = *new std::map<unsigned, std::pair<std::string, unsigned>>();

    if (!is_init) {
      for (auto entry : all_flag_to_parameter)
        all_parameter_to_flag[entry.second.first] = {entry.first, entry.second.second};
      is_init = true;
    }
    return all_parameter_to_flag;
  }
};
static const std::map<unsigned, std::pair<std::string, unsigned>> &all_parameter_to_flag =
    *new ParameterToFlag();

/// \brief Split a string into substrings on an input character
static std::vector<std::string> splitString(std::string str, char c) {
  std::vector<std::string> res;

  std::string buf;
  for (auto I = str.begin(); I != str.end(); ++I) {
    if (*I != c) {
      buf.push_back(*I);
    } else {
      res.push_back(buf);
      buf.clear();
    }
  }
  res.push_back(buf);
  return res;
}

/// \brief Print the version of this driver
static void printVersion() {
  mageec::util::out() << MAGEEC_PREFIX "Driver version: "
                      << static_cast<std::string>(gcc_driver_version) << '\n';
}

/// \brief Print the version of the database being used
static int printDatabaseVersion(mageec::Framework &framework,
                                const std::string &db_path) {
  std::unique_ptr<mageec::Database> db = framework.getDatabase(db_path, false);
  if (!db) {
    MAGEEC_ERR("Error retrieving database. The database may not exists, or "
               "you may not have sufficient permissions to read it");
    return -1;
  }
  mageec::util::out() << MAGEEC_PREFIX "Database version: "
                      << static_cast<std::string>(db->getVersion()) << '\n';
  return 0;
}

/// \brief Print the version of the framework that the driver was compiled for
static void printFrameworkVersion(mageec::Framework &framework) {
  mageec::util::out() << MAGEEC_PREFIX "Framework version: "
                      << static_cast<std::string>(framework.getVersion())
                      << '\n';
}


struct FeatureIDEntry {
  std::string            name;
  mageec::FeatureSetID   id;
  mageec::FeatureClass   feature_class;

  bool operator<(const FeatureIDEntry &other) const {
    if (name < other.name)
      return true;
    return false;
  }
};

struct FileFeatureIDs {
  mageec::util::Option<FeatureIDEntry> module;
  std::set<FeatureIDEntry>             functions;
};

/// \brief Load feature IDs from an input file
static mageec::util::Option<std::map<std::string, FileFeatureIDs>>
loadFeatureIDs(std::string features_path) {
  std::map<std::string, FileFeatureIDs> file_to_features;

  std::ifstream features_file(features_path);
  if (!features_file.is_open()) {
    MAGEEC_ERR("Error opening features file. The file may not exist, or you "
               "may not have sufficient permissions to read and write it");
    return nullptr;
  }

  std::string line;
  while (std::getline(features_file, line)) {
    std::vector<std::string> values = splitString(line, ',');
    if (values.size() != 7)
      continue;
    if ((values[1] != "module") && (values[1] != "function"))
      continue;
    if (values[3] != "features")
      continue;
    if (values[5] != "feature_class")
      continue;
    if (!values[0].size() || !values[2].size() || !values[4].size() ||
        !values[6].size()) {
      continue;
    }

    std::stringstream feat_id_str(values[4]);
    uint64_t feat_id;
    feat_id_str >> feat_id;
    if (feat_id_str.fail()) {
      MAGEEC_ERR("Malformed line in features file");
      return nullptr;
    }

    std::stringstream feat_class_str(values[6]);
    unsigned feat_class;
    feat_class_str >> feat_class;
    if (feat_class_str.fail()) {
      MAGEEC_ERR("Malformed line in features file");
      return nullptr;
    }

    // Entry to be inserted into the map
    FeatureIDEntry entry = { values[2],
                             static_cast<mageec::FeatureSetID>(feat_id),
                             static_cast<mageec::FeatureClass>(feat_class) };

    FileFeatureIDs &file_entry = file_to_features[values[0]];
    if (values[1] == "module") {
      if (file_entry.module) {
        FeatureIDEntry old_entry = file_entry.module.get();
        if (old_entry.id != entry.id
            || old_entry.feature_class != entry.feature_class) {
          MAGEEC_WARN("Multiple entries for module: " << entry.name
                      << " with different feature sets");
        }
      }
      file_entry.module = entry;
    } else {
      assert(values[1] == "function");
      if (file_entry.functions.count(entry)) {
        FeatureIDEntry old_entry = *file_entry.functions.find(entry);
        if (old_entry.id != entry.id
            || old_entry.feature_class != entry.feature_class) {
          MAGEEC_WARN("Multiple entries for function: " << entry.name
                      << " with different feature sets");
        }
      }
      file_entry.functions.insert(entry);
    }
  }
  return file_to_features;
}

/// \brief Print help output string
static void printHelp() {
  mageec::util::out() <<
"Wrapper around gcc which can interact with the mageec framework\n"
"\n"
"Basic options:\n"
"  -fmageec-help               Print this help information\n"
"  -fmageec-version            Print out the version of this driver\n"
"  -fmageec-database-version   Print the version of the provided database\n"
"  -fmageec-framework-version  Print the version of the MAGEEC framework\n"
"  -fmageec-debug              Enable debug output\n"
"  -fmageec-sql-trace          Enable tracing of any SQL queries run\n"
"  -fmageec-mode=<mode>        Mode of the driver, valid values are\n"
"                              gather and predict\n"
"  -fmageec-database=<file>    Database to record to\n"
"  -fmageec-features=<file>    File containing feature group identifiers\n"
"  -fmageec-out=<file>         File to output compilation ids into\n"
"  -fmageec-ml=<id>            string identifier or shared object identifying\n"
"                              the machine learner to be used\n"
"  -fmageec-metric=<name>      Metric to optimize for\n";
}

/// \brief Entry point for the GCC wrapper driver
int main(int argc, const char *argv[]) {
  DriverMode mode = DriverMode::kNone;

  // The database to gather into, or use when optimizing
  std::string db_str;
  // File holding the features for input programs
  std::string features_path;
  // File into which the compilation ids for the program should be output
  std::string out_path;
  // Params which this compilation will be compiled with
  std::vector<std::string> param_list;
  // The machine learner to optimize with
  std::string ml_str;
  // The metric to use when optimizing
  std::string metric_str;

  bool with_help              = false;
  bool with_version           = false;
  bool with_db_version        = false;
  bool with_framework_version = false;
  bool with_debug             = false;
  bool with_sql_trace         = false;
  bool with_db                = false;
  bool with_features          = false;
  bool with_out               = false;
  bool with_ml                = false;
  bool with_metric            = false;

  // Handle arguments controlling mageec, accumulate the arguments which
  // aren't controlling this driver
  std::vector<std::string> cmd_args;
  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.compare(0, strlen("-fmageec-"), "-fmageec-") != 0) {
      cmd_args.push_back(arg);
      continue;
    }
    arg = std::string(arg.begin() + strlen("-fmageec-"), arg.end());

    // Common flags
    bool handled = false;
    if (arg == "help") {
      handled = with_help = true;
    } else if (arg == "version") {
      handled = with_version = true;
    } else if (arg == "database-version") {
      handled = with_db_version = true;
    } else if (arg == "framework-version") {
      handled = with_framework_version = true;
    } else if (arg == "debug") {
      handled = with_debug = true;
    } else if (arg == "sql-trace") {
      handled = with_sql_trace = true;
    }
    if (handled)
      continue;

    // Flags with values
    if (arg.compare(0, strlen("mode="), "mode=") == 0) {
      std::string mode_str(arg.begin() + strlen("mode="), arg.end());

      if (mode_str == "gather") {
        mode = DriverMode::kGather;
      } else if (mode_str == "predict") {
        mode = DriverMode::kPredict;
      } else {
        MAGEEC_ERR("Unknown mode: '" << mode_str << "'");
        return -1;
      }
    } else if (arg.compare(0, strlen("database="), "database=") == 0) {
      db_str = std::string(arg.begin() + strlen("database="), arg.end());
      if (db_str == "") {
        MAGEEC_ERR("No database path provided");
        return -1;
      }
      with_db = true;
    } else if (arg.compare(0, strlen("features="), "features=") == 0) {
      features_path = std::string(arg.begin() + strlen("features="), arg.end());
      if (features_path.size() == 0) {
        MAGEEC_ERR("No feature path provided");
        return -1;
      }
      with_features = true;
    } else if (arg.compare(0, strlen("out="), "out=") == 0) {
      out_path = std::string(arg.begin() + strlen("out="), arg.end());
      if (out_path.size() == 0) {
        MAGEEC_ERR("No config file path provided");
        return -1;
      }
      with_out = true;
    } else if (arg.compare(0, strlen("ml="), "ml=") == 0) {
      ml_str = std::string(arg.begin() + strlen("ml="), arg.end());
      if (ml_str.size() == 0) {
        MAGEEC_ERR("No machine learner provided");
        return -1;
      }
      with_ml = true;
    } else if (arg.compare(0, strlen("metric="), "metric=") == 0) {
      metric_str = std::string(arg.begin() + strlen("metric="), arg.end());
      if (metric_str.size() == 0) {
        MAGEEC_ERR("No metric value provided");
        return -1;
      }
      with_metric = true;
    } else {
      MAGEEC_ERR("Unknown argument -fmageec-" << arg);
      return -1;
    }
  }

  // Errors
  bool have_error = false;
  if (mode == DriverMode::kPredict) {
    if (!with_db) {
      MAGEEC_ERR("Predict mode specified without a database");
      have_error = true;
    }
    if (!with_features) {
      MAGEEC_ERR("Predict mode specified without a features file");
      have_error = true;
    }
    if (!with_out) {
      MAGEEC_ERR("Predict mode specified without an output file");
      have_error = true;
    }
    if (!with_metric) {
      MAGEEC_ERR("Predict mode specified without any metric to optimize for");
      have_error = true;
    }
    if (!with_ml) {
      MAGEEC_ERR("Predict mode specified without a machine learner to use");
      have_error = true;
    }
  } else if (mode == DriverMode::kGather) {
    if (!with_db) {
      MAGEEC_ERR("Gather mode specified without a database");
      have_error = true;
    }
    if (!with_features) {
      MAGEEC_ERR("Gather mode specified without a features file ");
      have_error = true;
    }
    if (!with_out) {
      MAGEEC_ERR("Gather mode specified without an output file");
      have_error = true;
    }
  }
  if (have_error)
    return -1;

  // Warnings
  if (mode == DriverMode::kGather) {
    if (with_ml)
      MAGEEC_WARN("-fmageec-ml argument will be ignored");
    if (with_metric)
      MAGEEC_WARN("-fmageec-metric argument will be ignored");
  }

  // Get the underlying gcc command to be executed. Do this by stripping
  // everything before the 'mageec-' from the start of the name of the wrapper
  assert(cmd_args[0].find("mageec-") != std::string::npos);
  std::string gcc_command =
      cmd_args[0].substr(cmd_args[0].find("mageec-") + strlen("mageec="));

  // Run the underlying gcc command to determine the version of the
  // compiler being targetted
  std::array<char, 128> buffer;
  std::string version_str;

  std::string gcc_version_cmd = gcc_command + " -dumpversion";
  std::shared_ptr<FILE> pipe(popen(gcc_version_cmd.c_str(), "r"), pclose);
  if (!pipe) throw std::runtime_error("popen() failed!");
  while (!feof(pipe.get())) {
      if (fgets(buffer.data(), 128, pipe.get()) != NULL)
          version_str += buffer.data();
  }
  size_t idx = 0;
  int major = std::stoi(version_str, &idx);
  assert(version_str[idx] == '.');
  idx++;
  int minor = std::stoi(version_str.substr(idx), &idx);
  assert(version_str[idx] == '.');
  idx++;
  int patch = std::stoi(version_str.substr(idx), &idx);
  unsigned gcc_version = (major * 10000) + (minor + 100) + patch;

  if (gcc_version < 40500) {
    MAGEEC_ERR("GCC version '" + version_str + "' (>= 4.5.0 is required)");
    return -1;
  }

  // Use the GCC version to remove any flags and parameters which aren't
  // supported
  std::map<std::string, std::pair<unsigned, unsigned>> flag_to_parameter;
  std::map<unsigned, std::pair<std::string, unsigned>> parameter_to_flag;
  for (auto entry : all_flag_to_parameter)
    flag_to_parameter.insert(entry);
  for (auto entry : all_parameter_to_flag)
    parameter_to_flag.insert(entry);

  // Initialize the framework, and register some builtin machine learners so
  // they can be selected by name by the user.
  mageec::Framework framework(with_debug, with_sql_trace);

  // C5 classifier
  MAGEEC_DEBUG("Registering C5.0 machine learner interface");
  std::unique_ptr<mageec::IMachineLearner> c5_ml(new mageec::C5Driver());
  framework.registerMachineLearner(std::move(c5_ml));

  MAGEEC_DEBUG("Registering 1-NN machine learner interface");
  std::unique_ptr<mageec::IMachineLearner> nn_ml(new mageec::OneNN());
  framework.registerMachineLearner(std::move(nn_ml));

  // Select the machine learner chosen by the user. This may be the name of
  // an already register machine learner, or a path to a shared object which
  // needs to be loaded and registered.
  mageec::IMachineLearner *ml = nullptr;
  if (with_ml) {
    MAGEEC_DEBUG("Selecting machine learner: " << ml_str)

    // Try to select the machine learner assuming that it has already been
    // loaded
    std::set<mageec::IMachineLearner *> mls = framework.getMachineLearners();
    for (auto *ml_interface : mls) {
      if (ml_interface->getName() == ml_str) {
        ml = ml_interface;
        break;
      }
    }
    if (!ml) {
      MAGEEC_DEBUG(ml_str << " not a registered machine learner... attempting "
                   << "to load as a plugin");
      auto ml_name = framework.loadMachineLearner(ml_str);
      if (ml_name == "") {
        MAGEEC_ERR("Could not load user machine learner" << ml_str);
        return -1;
      } else {
        MAGEEC_DEBUG("Loaded machine learner plugin: " << ml_name);
      }
      std::set<mageec::IMachineLearner *> mls = framework.getMachineLearners();
      for (auto *ml_interface : mls) {
        if (ml_interface->getName() == ml_name) {
          ml = ml_interface;
          break;
        }
      }
      assert(ml);
    }
  }

  // Handle basic options
  if (with_help)
    printHelp();

  if (with_version)
    printVersion();

  if (with_db_version) {
    int res = printDatabaseVersion(framework, db_str);
    if (res != 0)
      return res;
  }
  if (with_framework_version)
    printFrameworkVersion(framework);

  // Arguments controlling mageec have been handled, now parse the underlying
  // gcc command line to find the input files and optimization flags used.
  bool to_obj = false;
  for (unsigned i = 0; i < cmd_args.size(); ++i) {
    if (cmd_args[i] == "-c")
      to_obj = true;

    // Check if the output file has a .o extension
    if (cmd_args[i] == "-o") {
      ++i;
      if (i < cmd_args.size()) {
        auto arg = cmd_args[i];
        if (arg.size() > strlen(".o")) {
          if (arg.compare(arg.size() - strlen(".o"), strlen(".o"), ".o") == 0)
            to_obj = true;
        }
      }
    }
    // A subsequent -S or -E override a preceding -c flag
    if (cmd_args[i] == "-S" || cmd_args[i] == "-E") {
      to_obj = false;
    }
  }

  // Replace the command word with the name of the underlying gcc command
  cmd_args[0] = gcc_command;

  // If we are not in 'gather' or 'predict' modes, or if we're not compiling
  // to an object file, then just run the original command
  if (!to_obj || (mode == DriverMode::kNone)) {
    std::stringstream command;
    for (unsigned i = 0; i < cmd_args.size(); ++i) {
      if (i == 0) {
        command << cmd_args[i];
      } else {
        command << " " << cmd_args[i];
      }
    }
    if (!to_obj && with_debug) {
      MAGEEC_WARN("MAGEEC driver called, but not compiling to an object file, "
                  "calling the original command");
    }
    if (with_debug) {
      MAGEEC_DEBUG("Executing command: " + command.str());
    }
    // FIXME: Windows?
    return system(command.str().c_str());
  }

  // Names of all of the input files involved in the compilation
  std::vector<std::string> src_files;

  // Find the input files and strip them from the command line (they will be
  // added back individually later).
  //
  // If an input file has an extension that we can't handle, then warn, but
  // pass it on to be compiled anyway.
  //
  // It's is assumed that anything without a preceding '-' is a filename,
  // excluding the command word, and the filename after the '-o' option.
  std::vector<std::string> new_cmd_args = {cmd_args[0]};
  auto arg_iter = std::next(cmd_args.begin());

  for (; arg_iter != cmd_args.end(); ++arg_iter) {
    auto arg = *arg_iter;

    // skip over the file after a -o argument
    if (!arg.compare(arg.size() - strlen("-o"), strlen("-o"), "-o")) {
      new_cmd_args.push_back(arg);
      if (std::next(arg_iter) != cmd_args.end()) {
        arg = *(++arg_iter);
        new_cmd_args.push_back(arg);
      }
      continue;
    }
    // ignore any other arguments beginning with '-', since they can't be
    // filenames (hopefully).
    if (arg[0] == '-') {
      new_cmd_args.push_back(arg);
      continue;
    }

    // Check the extension to see if it's a filename that can be handled
    // by the framework. If not, pass it on anyway, but warn in case it's
    // a case we haven't handled.
    const char *src_file_exts[] = {
      // C input file extensions
      ".c", ".i",
      // C++ input file extensions
      ".ii", ".cc", ".cp", ".cxx", ".cpp", ".CPP", ".c++", ".C",
      // Fortran input file extensions
      ".f", ".for", ".ftn", ".F", ".FOR", ".fpp", ".FPP", ".FTN", ".f90",
      ".f95", ".f03", ".f08", ".F90", ".F95", ".F03", ".F08",
      // Assembly code
      ".s", ".S", ".sx"
    };
    bool found_ext = false;
    for (auto ext : src_file_exts)
      found_ext |= arg.compare(arg.size() - strlen(ext), strlen(ext), ext) == 0;
    if (!found_ext)
      MAGEEC_WARN("Unrecognized extension on input file '" + arg + "'");

    if (with_debug)
      MAGEEC_DEBUG("Found input file '" + arg + "'");
    src_files.push_back(arg);
  };
  cmd_args = new_cmd_args;

  // Load the database
  assert((mode == DriverMode::kPredict) || (mode == DriverMode::kGather));
  std::unique_ptr<mageec::Database> db = framework.getDatabase(db_str, false);
  if (!db) {
    MAGEEC_ERR("Error retrieving database. The database may not exists, or "
               "you may not have sufficient permissions to read it");
    return -1;
  }

  // Load the features file to get the feature groups
  auto feature_groups = loadFeatureIDs(features_path);
  if (!feature_groups) {
    MAGEEC_ERR("Failed to retrieve feature groups from features file");
    return -1;
  }

  // Extract the parameters that were provided to the compiler on the command
  // line. Use this to build up a parameter set.
  std::set<unsigned> orig_params;

  // A copy of the command with the optimization flags stripped out
  std::vector<std::string> stripped_cmd_args;

  // First set the parameter set according to the basic optimization level
  // The rightmost optimization flag takes precedence.
  std::string base_opt = "-O0";
  for (auto arg : cmd_args) {
    if (arg == "-O0" || arg == "-O"  || arg == "-O1" || arg == "-O2" ||
        arg == "-O3" || arg == "-O4" || arg == "-Os" || arg == "-Ofast") {
      base_opt = arg;
    }
  }

  // flag -> parameter
  std::vector<std::string> flags;
  for (auto f : flags) {
    auto p = flag_to_parameter.find(f);
    if (p != flag_to_parameter.end())
      orig_params.insert(p->second.first);
  }

  // Now, toggle individual flags, modifying the base set of flags. The
  // rightmost flag takes precedence.
  //
  // Also build a stripped command, with all of the recognized optimization
  // flags omitted.
  for (auto arg : cmd_args) {
    if (arg == "-O0" || arg == "-O"  || arg == "-O1" || arg == "-O2" ||
        arg == "-O3" || arg == "-O4" || arg == "-Os" || arg == "-Ofast") {
      continue;
    }

    // Individual flags may enable the parameter, or disable it if it has a
    // "-fno-" prefix. First check if the flag enables a parameter, then if
    // it disables one.
    auto p = flag_to_parameter.find(arg);
    if (p != flag_to_parameter.end()) {
      orig_params.insert(p->second.first);
      continue;
    } else {
      if (arg.size() > strlen("-fno-") &&
          !arg.compare(0, strlen("-fno-"), "-fno-")) {
        arg = std::string("-f") +
              std::string(arg.begin() + strlen("-fno-"), arg.end());

        p = flag_to_parameter.find(arg);
        if (p != flag_to_parameter.end()) {
          orig_params.erase(p->second.first);
          continue;
        }
      }
    }
    // If this isn't an -O flag, or a parameter flag we recognize, add it to
    // the stripped command arguments
    stripped_cmd_args.push_back(arg);
  }

  auto src_file_feature_set_ids = feature_groups.get();
  std::map<std::string, std::set<unsigned>> src_file_parameters;
  std::map<std::string, mageec::ParameterSetID> src_file_parameter_set_ids;

  if (mode == DriverMode::kGather) {
    // When in 'gather' mode, the parameters used for each file are based on
    // the flags originally provided on the command line
    mageec::ParameterSet param_set;
    for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
         i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
      if (!parameter_to_flag.count(i))
        continue;
      auto param_flag = parameter_to_flag.at(i);
      assert(param_flag.second <= gcc_version);
      param_set.add(std::make_shared<mageec::BoolParameter>(i,
                                                            orig_params.count(i),
                                                            param_flag.first));
    }
    // Add the set of parameters to the database
    auto param_set_id = db->newParameterSet(param_set);

    // Use the same parameters for every input file
    for (auto file_arg : src_files) {
      auto src_file_path = mageec::util::getFullPath(file_arg);
      src_file_parameters[src_file_path] = orig_params;
      src_file_parameter_set_ids[src_file_path] = param_set_id;
    }
  } else {
    // When in 'predict' mode, the parameters used for each file are based on
    // flags predicted by the machine learner using the features
    assert(mode == DriverMode::kPredict);

    // Find the selected machine learner trained for the specified metric
    auto trained_mls = db->getTrainedMachineLearners();
    mageec::TrainedML *chosen_ml = nullptr;

    bool found_ml = false;
    for (auto &trained_ml : trained_mls) {
      if (trained_ml.getName() == ml->getName()) {
        found_ml = true;

        // Check that the found machine learner is trained for the desired
        // metric and class of features.
        // TODO: Only module features can be handled here
        if (trained_ml.getMetric() == metric_str &&
            trained_ml.getFeatureClass() == mageec::FeatureClass::kModule) {
          chosen_ml = &trained_ml;
          break;
        }
      }
    }
    if (!found_ml) {
      MAGEEC_ERR("Could not find training data for specified machine learner "
                 "and metric");
      return -1;
    }

    // For each input file, use the set of features for the file and the
    // user-specified machine learner to generate flags for the compilation
    for (auto file_arg : src_files) {
      auto src_file_path = mageec::util::getFullPath(file_arg);

      // Ignore any files which don't have associated features, these will
      // be built with the original command line
      auto feature_set_ids = src_file_feature_set_ids.find(src_file_path);
      if (feature_set_ids == src_file_feature_set_ids.end())
        continue;

      // Generate a parameter set based on the features of the module for the
      // file
      //
      // Use the original parameter configuration provided on the command line
      // as the base set of flags. If a parameter value is not overridden by
      // mageec then this will form the 'native' decision
      assert(feature_set_ids->second.module);
      auto feature_set_id = feature_set_ids->second.module.get().id;
      auto features = db->getFeatureSetFeatures(feature_set_id);
      assert(features.size() != 0);

      std::set<unsigned> params;
      mageec::ParameterSet param_set;
      for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
           i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
        assert(chosen_ml);

        mageec::BoolDecisionRequest req(i);
        auto res = chosen_ml->makeDecision(req, features);

        bool enabled = false;
        if (res->getType() == mageec::DecisionType::kNative) {
          enabled = orig_params.count(i);
        } else {
          auto *decision = static_cast<mageec::BoolDecision*>(res.get());
          enabled = decision->getValue();
        }

        if (!parameter_to_flag.count(i))
          continue;
        auto param_flag = parameter_to_flag.at(i);
        assert(param_flag.second <= gcc_version);
        param_set.add(std::make_shared<mageec::BoolParameter>(i, enabled,
                                                              param_flag.first));
        if (enabled)
          params.insert(i);
      }
      // Add the set of parameters to the database
      auto param_set_id = db->newParameterSet(param_set);

      src_file_parameters[src_file_path] = params;
      src_file_parameter_set_ids[src_file_path] = param_set_id;
    }
  }

  // Mapping from an input filename, to a command string to compile that file
  // With the appropriate set of parameters.
  std::map<std::string, std::string> src_file_commands;

  for (auto file_arg : src_files) {
    auto src_file_path = mageec::util::getFullPath(file_arg);

    // If this file doesn't have any features, then it cannot be affected by
    // mageec. Just use the original command with the input filename appended
    if (src_file_feature_set_ids.count(src_file_path) == 0) {
      std::stringstream command;
      for (unsigned i = 0; i < cmd_args.size(); ++i) {
        if (i == 0)
          command << cmd_args[i];
        else
          command << " " << cmd_args[i];
      }
      // Add in the input file
      command << " " << file_arg;

      src_file_commands[src_file_path] = command.str();
      continue;
    }

    // Otherwise, this file has features, and therefore will also have
    // associated parameters. Use these parameters to build up a new
    // command line
    auto params = src_file_parameters[src_file_path];

    // The parameters kBranchTargetLoadOptimize and kBranchTargetLoadOptimize2
    // can't *both* be run, so disable the second run to avoid triggering a
    // compiler warning.
    if (params.count(FlagParameterID::kBranchTargetLoadOptimize) &&
        params.count(FlagParameterID::kBranchTargetLoadOptimize2)) {
      params.erase(FlagParameterID::kBranchTargetLoadOptimize2);
    }

    // Build the command line for this file based on the 'stripped' command
    // line derived earlier, with the specified optimization flags enabled
    // or disabled.
    auto cmd_iter = stripped_cmd_args.begin();
    std::vector<std::string> file_cmd;

    // Add the original command word
    file_cmd.push_back(*cmd_iter);
    ++cmd_iter;

    // Earlier the 'base' optimization level (-Os, -O3, etc) was extracted.
    // This is now added back to the flags.
    // TODO: Record the base optimization level in the database so that we
    // can enforce the same base optimization level between the 'gather'
    // and 'predict' stages.
    file_cmd.push_back(base_opt);

    // Add the optimization flags to the command. This adds *all* optimization
    // flags which the driver knows about, either in -fflag-name or
    // -fno-flag-name form depending on whether the flag is enabled or
    // disabled
    for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
         i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
      if (!parameter_to_flag.count(i))
        continue;
      auto entry = parameter_to_flag.at(i);
      assert(entry.second <= gcc_version);
      std::string flag = entry.first;

      if (params.count(i)) {
        file_cmd.push_back(flag);
      } else {
        assert(!flag.compare(0, strlen("-f"), "-f"));
        flag = std::string("-fno-") +
               std::string(flag.begin() + strlen("-f"), flag.end());
        file_cmd.push_back(flag);
      }
    }

    // Add the remaining flags from the original command
    for (; cmd_iter != stripped_cmd_args.end(); ++cmd_iter)
      file_cmd.push_back(*cmd_iter);

    // Add the input filename
    file_cmd.push_back(file_arg);

    // Build a command appropriate to compile each of the files
    std::stringstream command;
    for (unsigned i = 0; i < file_cmd.size(); ++i) {
      if (i == 0)
        command << file_cmd[i];
      else
        command << " " << file_cmd[i];
    }
    src_file_commands[src_file_path] = command.str();
  }

  // Compile each of the files in turn, if any fail, error out early
  for (auto file_arg : src_files) {
    auto src_file_path = mageec::util::getFullPath(file_arg);
    auto command = src_file_commands[src_file_path];

    // FIXME: Windows?
    MAGEEC_DEBUG("Executing command: " << command);
    int res = system(command.c_str());
    if (res) {
      MAGEEC_ERR("Compilation failed\ncommand: " << command);
      return res;
    }
  }

  // If all of the file compiled successfully, generated compilation ids for
  // them and output these ids into the output file
  std::ofstream out_file(out_path, std::ios::app);
  if (!out_file.is_open()) {
    MAGEEC_ERR("Error opening output file. The file may not exist, or you "
               "may not have sufficient permissions to read and write it");
    return -1;
  }
  for (auto file_arg : src_files) {
    std::string src_file_path = mageec::util::getFullPath(file_arg);
    auto feature_set_ids = src_file_feature_set_ids.find(src_file_path);
    auto param_set_id = src_file_parameter_set_ids.find(src_file_path);

    // If there were no features for this file, then parameters would not have
    // been derived and there will be no compilation id
    if (feature_set_ids == src_file_feature_set_ids.end())
      continue;
    assert(param_set_id != src_file_parameter_set_ids.end());

    // Generate a compilation id for the module
    // Append the generated compilation ids to the output file
    assert(feature_set_ids->second.module);
    auto module_entry = feature_set_ids->second.module.get();
    auto module_compilation = db->newCompilation(module_entry.name, "module",
                                                 module_entry.id,
                                                 mageec::FeatureClass::kModule,
                                                 param_set_id->second,
                                                 // FIXME: The compilation command
                                                 // takes up a lot of space so
                                                 // we don't store it for now
                                                 nullptr, //src_file_commands[src_file_path],
                                                 nullptr);

    // TODO: Avoid static_cast here
    uint64_t tmp = static_cast<uint64_t>(module_compilation);
    out_file << src_file_path << ",module," << module_entry.name
                               << ",compilation," << tmp << "\n";

    // Generate a compilation id for each of the functions in the module.
    for (auto function_entry : feature_set_ids->second.functions) {
      auto function_compilation =
          db->newCompilation(function_entry.name, "function",
                             function_entry.id, mageec::FeatureClass::kFunction,
                             param_set_id->second,
                             // FIXME: The compilation command takes up a lot of
                             // space so we don't store it for now.
                             nullptr, //src_file_commands[src_file_path],
                             module_compilation);

      // TODO: Avoid static cast here
      tmp = static_cast<uint64_t>(function_compilation);
      out_file << src_file_path << ",function," << function_entry.name
                                 << ",compilation," << tmp << "\n";
    }
  }
  return 0;
}
