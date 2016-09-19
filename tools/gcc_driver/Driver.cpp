#include "mageec/Attribute.h"
#include "mageec/Database.h"
#include "mageec/Framework.h"
#include "mageec/ML/C5.h"
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

enum class DriverMode { kNone, kGather, kOptimize };

static const auto &opt_flags_O0 = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fdce",
    "-fdelete-null-pointer-checks",
    "-fdse",
    "-fearly-inlining",
    "-ffunction-cse",
    "-fgcse-lm",
    "-finline",
    "-finline-atomics",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-fmath-errno",
    "-fpeephole",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fshort-enums",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fstdarg-opt",
    "-fstrict-volatile-bitfields",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-coalesce-vars",
    "-ftree-cselim",
    "-ftree-forwprop",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-phiprop",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_O1 = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdse",
    "-fearly-inlining",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fif-conversion",
    "-fif-conversion2",
    "-finline",
    "-finline-atomics",
    "-finline-functions-called-once",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-reference",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-fmath-errno",
    "-fmove-loop-invariants",
    "-fpeephole",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-volatile-bitfields",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-bit-ccp",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-phiprop",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-ter",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_O2 = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-falign-functions",
    "-falign-jumps",
    "-falign-labels",
    "-falign-loops",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcaller-saves",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fcrossjumping",
    "-fcse-follow-jumps",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdevirtualize",
    "-fdevirtualize-speculatively",
    "-fdse",
    "-fearly-inlining",
    "-fexpensive-optimizations",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fhoist-adjacent-loads",
    "-fif-conversion",
    "-fif-conversion2",
    "-findirect-inlining",
    "-finline",
    "-finline-atomics",
    "-finline-functions-called-once",
    "-finline-small-functions",
    "-fipa-cp",
    "-fipa-cp-alignment",
    "-fipa-icf",
    "-fipa-icf-functions",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-ra",
    "-fipa-reference",
    "-fipa-sra",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fisolate-erroneous-paths-dereference",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-flra-remat",
    "-fmath-errno",
    "-fmove-loop-invariants",
    "-foptimize-sibling-calls",
    "-foptimize-strlen",
    "-fpartial-inlining",
    "-fpeephole",
    "-fpeephole2",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-freorder-blocks",
    "-freorder-blocks-and-partition",
    "-freorder-functions",
    "-frerun-cse-after-loop",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fschedule-insns2",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-aliasing",
    "-fstrict-overflow",
    "-fstrict-volatile-bitfields",
    "-fthread-jumps",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-bit-ccp",
    "-ftree-builtin-call-dce",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-phiprop",
    "-ftree-pre",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-switch-conversion",
    "-ftree-tail-merge",
    "-ftree-ter",
    "-ftree-vrp",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_O3 = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-falign-functions",
    "-falign-jumps",
    "-falign-labels",
    "-falign-loops",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcaller-saves",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fcrossjumping",
    "-fcse-follow-jumps",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdevirtualize",
    "-fdevirtualize-speculatively",
    "-fdse",
    "-fearly-inlining",
    "-fexpensive-optimizations",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse",
    "-fgcse-after-reload",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fhoist-adjacent-loads",
    "-fif-conversion",
    "-fif-conversion2",
    "-findirect-inlining",
    "-finline",
    "-finline-atomics",
    "-finline-functions",
    "-finline-functions-called-once",
    "-finline-small-functions",
    "-fipa-cp",
    "-fipa-cp-alignment",
    "-fipa-cp-clone",
    "-fipa-icf",
    "-fipa-icf-functions",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-ra",
    "-fipa-reference",
    "-fipa-sra",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fisolate-erroneous-paths-dereference",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-flra-remat",
    "-fmath-errno",
    "-fmove-loop-invariants",
    "-foptimize-sibling-calls",
    "-foptimize-strlen",
    "-fpartial-inlining",
    "-fpeephole",
    "-fpeephole2",
    "-fpredictive-commoning",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-freorder-blocks",
    "-freorder-blocks-and-partition",
    "-freorder-functions",
    "-frerun-cse-after-loop",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fschedule-insns2",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-aliasing",
    "-fstrict-overflow",
    "-fstrict-volatile-bitfields",
    "-fthread-jumps",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-bit-ccp",
    "-ftree-builtin-call-dce",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-distribute-patterns",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-loop-vectorize",
    "-ftree-partial-pre",
    "-ftree-phiprop",
    "-ftree-pre",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slp-vectorize",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-switch-conversion",
    "-ftree-tail-merge",
    "-ftree-ter",
    "-ftree-vrp",
    "-funswitch-loops",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_O4 = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-falign-functions",
    "-falign-jumps",
    "-falign-labels",
    "-falign-loops",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcaller-saves",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fcrossjumping",
    "-fcse-follow-jumps",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdevirtualize",
    "-fdevirtualize-speculatively",
    "-fdse",
    "-fearly-inlining",
    "-fexpensive-optimizations",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse",
    "-fgcse-after-reload",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fhoist-adjacent-loads",
    "-fif-conversion",
    "-fif-conversion2",
    "-findirect-inlining",
    "-finline",
    "-finline-atomics",
    "-finline-functions",
    "-finline-functions-called-once",
    "-finline-small-functions",
    "-fipa-cp",
    "-fipa-cp-alignment",
    "-fipa-cp-clone",
    "-fipa-icf",
    "-fipa-icf-functions",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-ra",
    "-fipa-reference",
    "-fipa-sra",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fisolate-erroneous-paths-dereference",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-flra-remat",
    "-fmath-errno",
    "-fmove-loop-invariants",
    "-foptimize-sibling-calls",
    "-foptimize-strlen",
    "-fpartial-inlining",
    "-fpeephole",
    "-fpeephole2",
    "-fpredictive-commoning",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-freorder-blocks",
    "-freorder-blocks-and-partition",
    "-freorder-functions",
    "-frerun-cse-after-loop",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fschedule-insns2",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-aliasing",
    "-fstrict-overflow",
    "-fstrict-volatile-bitfields",
    "-fthread-jumps",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-bit-ccp",
    "-ftree-builtin-call-dce",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-distribute-patterns",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-loop-vectorize",
    "-ftree-partial-pre",
    "-ftree-phiprop",
    "-ftree-pre",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slp-vectorize",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-switch-conversion",
    "-ftree-tail-merge",
    "-ftree-ter",
    "-ftree-vrp",
    "-funswitch-loops",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_Os = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-falign-functions",
    "-falign-jumps",
    "-falign-labels",
    "-falign-loops",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcaller-saves",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fcrossjumping",
    "-fcse-follow-jumps",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdevirtualize",
    "-fdevirtualize-speculatively",
    "-fdse",
    "-fearly-inlining",
    "-fexpensive-optimizations",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fhoist-adjacent-loads",
    "-fif-conversion",
    "-fif-conversion2",
    "-findirect-inlining",
    "-finline",
    "-finline-atomics",
    "-finline-functions",
    "-finline-functions-called-once",
    "-finline-small-functions",
    "-fipa-cp",
    "-fipa-cp-alignment",
    "-fipa-icf",
    "-fipa-icf-functions",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-ra",
    "-fipa-reference",
    "-fipa-sra",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fisolate-erroneous-paths-dereference",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-flra-remat",
    "-fmath-errno",
    "-fmove-loop-invariants",
    "-foptimize-sibling-calls",
    "-fpartial-inlining",
    "-fpeephole",
    "-fpeephole2",
    "-fprefetch-loop-arrays",
    "-frename-registers",
    "-freorder-blocks",
    "-freorder-blocks-and-partition",
    "-freorder-functions",
    "-frerun-cse-after-loop",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fschedule-insns2",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsigned-zeros",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-aliasing",
    "-fstrict-overflow",
    "-fstrict-volatile-bitfields",
    "-fthread-jumps",
    "-fno-threadsafe-statics",
    "-ftrapping-math",
    "-ftree-bit-ccp",
    "-ftree-builtin-call-dce",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-phiprop",
    "-ftree-pre",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-switch-conversion",
    "-ftree-tail-merge",
    "-ftree-ter",
    "-ftree-vrp",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

static const auto &opt_flags_Ofast = *new std::vector<std::string>({
    "-faggressive-loop-optimizations",
    "-falign-functions",
    "-falign-jumps",
    "-falign-labels",
    "-falign-loops",
    "-fassociative-math",
    "-fasynchronous-unwind-tables",
    "-fauto-inc-dec",
    "-fbranch-count-reg",
    "-fcaller-saves",
    "-fcombine-stack-adjustments",
    "-fcompare-elim",
    "-fcprop-registers",
    "-fcrossjumping",
    "-fcse-follow-jumps",
    "-fcx-limited-range",
    "-fdce",
    "-fdefer-pop",
    "-fdelete-null-pointer-checks",
    "-fdevirtualize",
    "-fdevirtualize-speculatively",
    "-fdse",
    "-fearly-inlining",
    "-fexpensive-optimizations",
    "-ffinite-math-only",
    "-fforward-propagate",
    "-ffunction-cse",
    "-fgcse",
    "-fgcse-after-reload",
    "-fgcse-lm",
    "-fguess-branch-probability",
    "-fhoist-adjacent-loads",
    "-fif-conversion",
    "-fif-conversion2",
    "-findirect-inlining",
    "-finline",
    "-finline-atomics",
    "-finline-functions",
    "-finline-functions-called-once",
    "-finline-small-functions",
    "-fipa-cp",
    "-fipa-cp-alignment",
    "-fipa-cp-clone",
    "-fipa-icf",
    "-fipa-icf-functions",
    "-fipa-profile",
    "-fipa-pure-const",
    "-fipa-ra",
    "-fipa-reference",
    "-fipa-sra",
    "-fira-hoist-pressure",
    "-fira-share-save-slots",
    "-fira-share-spill-slots",
    "-fisolate-erroneous-paths-dereference",
    "-fivopts",
    "-fjump-tables",
    "-flifetime-dse",
    "-flra-remat",
    "-fmove-loop-invariants",
    "-foptimize-sibling-calls",
    "-foptimize-strlen",
    "-fpartial-inlining",
    "-fpeephole",
    "-fpeephole2",
    "-fpredictive-commoning",
    "-fprefetch-loop-arrays",
    "-freciprocal-math",
    "-frename-registers",
    "-freorder-blocks",
    "-freorder-blocks-and-partition",
    "-freorder-functions",
    "-frerun-cse-after-loop",
    "-frtti",
    "-fsched-critical-path-heuristic",
    "-fsched-dep-count-heuristic",
    "-fsched-group-heuristic",
    "-fsched-interblock",
    "-fsched-last-insn-heuristic",
    "-fsched-rank-heuristic",
    "-fsched-spec",
    "-fsched-spec-insn-heuristic",
    "-fsched-stalled-insns-dep",
    "-fschedule-fusion",
    "-fschedule-insns2",
    "-fshort-enums",
    "-fshrink-wrap",
    "-fsplit-ivs-in-unroller",
    "-fsplit-wide-types",
    "-fssa-phiopt",
    "-fstdarg-opt",
    "-fstrict-aliasing",
    "-fstrict-overflow",
    "-fstrict-volatile-bitfields",
    "-fthread-jumps",
    "-fno-threadsafe-statics",
    "-ftree-bit-ccp",
    "-ftree-builtin-call-dce",
    "-ftree-ccp",
    "-ftree-ch",
    "-ftree-coalesce-vars",
    "-ftree-copy-prop",
    "-ftree-copyrename",
    "-ftree-cselim",
    "-ftree-dce",
    "-ftree-dominator-opts",
    "-ftree-dse",
    "-ftree-forwprop",
    "-ftree-fre",
    "-ftree-loop-distribute-patterns",
    "-ftree-loop-if-convert",
    "-ftree-loop-im",
    "-ftree-loop-ivcanon",
    "-ftree-loop-optimize",
    "-ftree-loop-vectorize",
    "-ftree-partial-pre",
    "-ftree-phiprop",
    "-ftree-pre",
    "-ftree-pta",
    "-ftree-reassoc",
    "-ftree-scev-cprop",
    "-ftree-sink",
    "-ftree-slp-vectorize",
    "-ftree-slsr",
    "-ftree-sra",
    "-ftree-switch-conversion",
    "-ftree-tail-merge",
    "-ftree-ter",
    "-ftree-vrp",
    "-funsafe-math-optimizations",
    "-funswitch-loops",
    "-fvar-tracking",
    "-fvar-tracking-assignments",
    "-fweb"});

// Heap allocate so the destructor does not need to be called
static const auto &flag_to_parameter = *new std::map<std::string, unsigned>({
  {"-faggressive-loop-optimizations",      FlagParameterID::kAggressiveLoopOptimizations},
  {"-falign-functions",                    FlagParameterID::kAlignFunctions},
  {"-falign-jumps",                        FlagParameterID::kAlignJumps},
  {"-falign-labels",                       FlagParameterID::kAlignLabels},
  {"-falign-loops",                        FlagParameterID::kAlignLoops},
  {"-fbranch-count-reg",                   FlagParameterID::kBranchCountReg},
  {"-fbranch-target-load-optimize",        FlagParameterID::kBranchTargetLoadOptimize},
  {"-fbranch-target-load-optimize2",       FlagParameterID::kBranchTargetLoadOptimize2},
  {"-fbtr-bb-exclusive",                   FlagParameterID::kBTRBBExclusive},
  {"-fcaller-saves",                       FlagParameterID::kCallerSaves},
  {"-fcombine-stack-adjustments",          FlagParameterID::kCombineStackAdjustments},
  {"-fcommon",                             FlagParameterID::kCommon},
  {"-fcompare-elim",                       FlagParameterID::kCompareElim},
  {"-fconserve-stack",                     FlagParameterID::kConserveStack},
  {"-fcprop-registers",                    FlagParameterID::kCPropRegister},
  {"-fcrossjumping",                       FlagParameterID::kCrossJumping},
  {"-fcse-follow-jumps",                   FlagParameterID::kCSEFollowJumps},
  {"-fdata-sections",                      FlagParameterID::kDataSections},
  {"-fdce",                                FlagParameterID::kDCE},
  {"-fdefer-pop",                          FlagParameterID::kDeferPop},
  {"-fdelete-null-pointer-checks",         FlagParameterID::kDeleteNullPointerChecks},
  {"-fdevirtualize",                       FlagParameterID::kDevirtualize},
  {"-fdse",                                FlagParameterID::kDSE},
  {"-fearly-inlining",                     FlagParameterID::kEarlyInlining},
  {"-fexpensive-optimizations",            FlagParameterID::kExpensiveOptimizations},
  {"-fforward-propagate",                  FlagParameterID::kForwardPropagate},
  {"-fgcse",                               FlagParameterID::kGCSE},
  {"-fgcse-after-reload",                  FlagParameterID::kGCSEAfterReload},
  {"-fgcse-las",                           FlagParameterID::kGCSELAS},
  {"-fgcse-lm",                            FlagParameterID::kGCSELM},
  {"-fgcse-sm",                            FlagParameterID::kGCSESM},
  {"-fguess-branch-probability",           FlagParameterID::kGuessBranchProbability},
  {"-fhoist-adjacent-loads",               FlagParameterID::kHoistAdjacentLoads},
  {"-fif-conversion",                      FlagParameterID::kIfConversion},
  {"-fif-conversion2",                     FlagParameterID::kIfConversion2},
  {"-finline",                             FlagParameterID::kInline},
  {"-finline-atomics",                     FlagParameterID::kInlineAtomics},
  {"-finline-functions",                   FlagParameterID::kInlineFunctions},
  {"-finline-functions-called-once",       FlagParameterID::kInlineFunctionsCalledOnce},
  {"-finline-small-functions",             FlagParameterID::kInlineSmallFunctions},
  {"-fipa-cp",                             FlagParameterID::kIPACP},
  {"-fipa-cp-clone",                       FlagParameterID::kIPACPClone},
  {"-fipa-profile",                        FlagParameterID::kIPAProfile},
  {"-fipa-pta",                            FlagParameterID::kIPAPTA},
  {"-fipa-pure-const",                     FlagParameterID::kIPAPureConst},
  {"-fipa-reference",                      FlagParameterID::kIPAReference},
  {"-fipa-sra",                            FlagParameterID::kIPASRA},
  {"-fira-hoist-pressure",                 FlagParameterID::kIRAHoistPressure},
  {"-fivopts",                             FlagParameterID::kIVOpts},
  {"-fmerge-constants",                    FlagParameterID::kMergeConstants},
  {"-fmodulo-sched",                       FlagParameterID::kModuloSched},
  {"-fmove-loop-invariants",               FlagParameterID::kMoveLoopInvariants},
  {"-fomit-frame-pointer",                 FlagParameterID::kOmitFramePointer},
  {"-foptimize-sibling-calls",             FlagParameterID::kOptimizeSiblingCalls},
  {"-foptimize-strlen",                    FlagParameterID::kOptimizeStrLen},
  {"-fpeephole",                           FlagParameterID::kPeephole},
  {"-fpeephole2",                          FlagParameterID::kPeephole2},
  {"-fpredictive-commoning",               FlagParameterID::kPredictiveCommoning},
  {"-fprefetch-loop-arrays",               FlagParameterID::kPrefetchLoopArrays},
  {"-fregmove",                            FlagParameterID::kRegMove},
  {"-frename-registers",                   FlagParameterID::kRenameRegisters},
  {"-freorder-blocks",                     FlagParameterID::kReorderBlocks},
  {"-freorder-functions",                  FlagParameterID::kReorderFunctions},
  {"-frerun-cse-after-loop",               FlagParameterID::kRerunCSEAfterLoop},
  {"-freschedule-modulo-scheduled-loops",  FlagParameterID::kRescheduleModuloScheduledLoops},
  {"-fsched-critical-path-heuristic",      FlagParameterID::kSchedCriticalPathHeuristic},
  {"-fsched-dep-count-heuristic",          FlagParameterID::kSchedDepCountHeuristic},
  {"-fsched-group-heuristic",              FlagParameterID::kSchedGroupHeuristic},
  {"-fsched-interblock",                   FlagParameterID::kSchedInterblock},
  {"-fsched-last-insn-heuristic",          FlagParameterID::kSchedLastInsnHeuristic},
  {"-fsched-pressure",                     FlagParameterID::kSchedPressure},
  {"-fsched-rank-heuristic",               FlagParameterID::kSchedRankHeuristic},
  {"-fsched-spec",                         FlagParameterID::kSchedSpec},
  {"-fsched-spec-insn-heuristic",          FlagParameterID::kSchedSpecInsnHeuristic},
  {"-fsched-spec-load",                    FlagParameterID::kSchedSpecLoad},
  {"-fsched-stalled-insns",                FlagParameterID::kSchedStalledInsns},
  {"-fsched-stalled-insns-dep",            FlagParameterID::kSchedStalledInsnsDep},
  {"-fschedule-insns",                     FlagParameterID::kScheduleInsns},
  {"-fschedule-insns2",                    FlagParameterID::kScheduleInsns2},
  {"-fsection-anchors",                    FlagParameterID::kSectionAnchors},
  {"-fsel-sched-pipelining",               FlagParameterID::kSelSchedPipelining},
  {"-fsel-sched-pipelining-outer-loops",   FlagParameterID::kSelSchedPipeliningOuterLoops},
  {"-fsel-sched-reschedule-pipelined",     FlagParameterID::kSelSchedReschedulePipelined},
  {"-fselective-scheduling",               FlagParameterID::kSelectiveScheduling},
  {"-fselective-scheduling2",              FlagParameterID::kSelectiveScheduling2},
  {"-fshrink-wrap",                        FlagParameterID::kShrinkWrap},
  {"-fsplit-ivs-in-unroller",              FlagParameterID::kSplitIVsInUnroller},
  {"-fsplit-wide-types",                   FlagParameterID::kSplitWideTypes},
  {"-fstrict-aliasing",                    FlagParameterID::kStrictAliasing},
  {"-fthread-jumps",                       FlagParameterID::kThreadJumps},
  {"-ftoplevel-reorder",                   FlagParameterID::kTopLevelReorder},
  {"-ftree-bit-ccp",                       FlagParameterID::kTreeBitCCP},
  {"-ftree-builtin-call-dce",              FlagParameterID::kTreeBuiltinCallDCE},
  {"-ftree-ccp",                           FlagParameterID::kTreeCCP},
  {"-ftree-ch",                            FlagParameterID::kTreeCH},
  {"-ftree-coalesce-inlined-vars",         FlagParameterID::kTreeCoalesceInlinedVars},
  {"-ftree-coalesce-vars",                 FlagParameterID::kTreeCoalesceVars},
  {"-ftree-copy-prop",                     FlagParameterID::kTreeCopyProp},
  {"-ftree-copyrename",                    FlagParameterID::kTreeCopyRename},
  {"-ftree-cselim",                        FlagParameterID::kTreeCSEElim},
  {"-ftree-dce",                           FlagParameterID::kTreeDCE},
  {"-ftree-dominator-opts",                FlagParameterID::kTreeDominatorOpts},
  {"-ftree-dse",                           FlagParameterID::kTreeDSE},
  {"-ftree-forwprop",                      FlagParameterID::kTreeForwProp},
  {"-ftree-fre",                           FlagParameterID::kTreeFRE},
  {"-ftree-loop-distribute-patterns",      FlagParameterID::kTreeLoopDistributePatterns},
  {"-ftree-loop-distribution",             FlagParameterID::kTreeLoopDistribution},
  {"-ftree-loop-if-convert",               FlagParameterID::kTreeLoopIfConvert},
  {"-ftree-loop-im",                       FlagParameterID::kTreeLoopIM},
  {"-ftree-loop-ivcanon",                  FlagParameterID::kTreeLoopIVCanon},
  {"-ftree-loop-optimize",                 FlagParameterID::kTreeLoopOptimize},
  {"-ftree-partial-pre",                   FlagParameterID::kTreePartialPre},
  {"-ftree-phiprop",                       FlagParameterID::kTreePhiProp},
  {"-ftree-pre",                           FlagParameterID::kTreePre},
  {"-ftree-pta",                           FlagParameterID::kTreePTA},
  {"-ftree-reassoc",                       FlagParameterID::kTreeReassoc},
  {"-ftree-scev-cprop",                    FlagParameterID::kTreeSCEVCProp},
  {"-ftree-sink",                          FlagParameterID::kTreeSink},
  {"-ftree-slp-vectorize",                 FlagParameterID::kTreeSLPVectorize},
  {"-ftree-slsr",                          FlagParameterID::kTreeSLSR},
  {"-ftree-sra",                           FlagParameterID::kTreeSRA},
  {"-ftree-switch-conversion",             FlagParameterID::kTreeSwitchConversion},
  {"-ftree-tail-merge",                    FlagParameterID::kTreeTailMerge},
  {"-ftree-ter",                           FlagParameterID::kTreeTER},
  {"-ftree-vect-loop-version",             FlagParameterID::kTreeVectLoopVersion},
  {"-ftree-vectorize",                     FlagParameterID::kTreeVectorize},
  {"-ftree-vrp",                           FlagParameterID::kTreeVRP},
  {"-funroll-all-loops",                   FlagParameterID::kUnrollAllLoops},
  {"-funroll-loops",                       FlagParameterID::kUnrollLoops},
  {"-funswitch-loops",                     FlagParameterID::kUnswitchLoops},
  {"-fvariable-expansion-in-unroller",     FlagParameterID::kVariableExpansionInUnroller},
  {"-fvect-cost-model",                    FlagParameterID::kVectCostModel},
  {"-fweb",                                FlagParameterID::kWeb},
});

struct ParameterToFlag {
  ParameterToFlag() {}
  operator std::map<unsigned, std::string>() {
    static bool is_init = false;
    static auto &parameter_to_flag = *new std::map<unsigned, std::string>();

    if (!is_init) {
      for (auto entry : flag_to_parameter)
        parameter_to_flag[entry.second] = entry.first;
      is_init = true;
    }
    return parameter_to_flag;
  }
};
static const std::map<unsigned, std::string> &parameter_to_flag =
    *new ParameterToFlag();

// Split a string on a provided character
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

// Print the version of this driver.
static void printVersion() {
  mageec::util::out() << MAGEEC_PREFIX "Driver version: "
                      << static_cast<std::string>(gcc_driver_version) << '\n';
}

// Print the version of the database that was specified for use
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

// Print the version of the framework that the driver was compiled with.
static void printFrameworkVersion(mageec::Framework &framework) {
  mageec::util::out() << MAGEEC_PREFIX "Framework version: "
                      << static_cast<std::string>(framework.getVersion())
                      << '\n';
}

static mageec::util::Option<std::map<std::string, mageec::FeatureGroupID>>
loadConfigFile(std::string config_path) {
  std::map<std::string, mageec::FeatureGroupID> file_to_features;

  std::ifstream config_file(config_path);
  if (!config_file.is_open()) {
    MAGEEC_ERR("Error opening config file. The file may not exist, or you "
               "may not have sufficient permissions to read and write it");
    return nullptr;
  }

  std::string line;
  while (std::getline(config_file, line)) {
    std::vector<std::string> values = splitString(line, ',');
    if (values.size() != 5)
      continue;
    if ((values[1] != "module") || (values[3] != "feature_group"))
      continue;
    if (!values[0].size() || !values[2].size() || !values[4].size())
      continue;

    // store the mapping from the source file name to the feature group
    std::string file_path = values[0];
    if (file_to_features.count(file_path)) {
      MAGEEC_ERR("Multiple module entries for a file");
      return nullptr;
    }
    std::stringstream feat_id_str(values[4]);
    uint64_t feat_id;
    feat_id_str >> feat_id;
    if (feat_id_str.fail()) {
      MAGEEC_ERR("Malformed line in config file");
      return nullptr;
    }

    file_to_features[file_path] = static_cast<mageec::FeatureGroupID>(feat_id);
  }
  return file_to_features;
}

// Print the help output string
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
"  -fmageec-mode=<mode>        Mode of the driver, valid values are\n"
"                              gather and optimize\n"
"  -fmageec-database=<file>    Database to record to\n"
"  -fmageec-config=<file>      File containing feature group identifiers,\n"
"                              and to output compilation ids to\n"
"  -fmageec-ml=<id>            UUID or shared object identifying the machine\n"
"                              learner to be used\n"
"  -fmageec-metric=<name>      Metric to optimize for\n";
}

int main(int argc, const char *argv[]) {
  DriverMode mode = DriverMode::kNone;

  // The database to gather into, or use when optimizing
  std::string db_str;
  // Config file holding the features for the input program, and which will
  // hold the compilation ids for the compiled program.
  std::string config_path;
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
  bool with_db                = false;
  bool with_config            = false;
  bool with_ml                = false;
  bool with_metric            = false;

  // Handle arguments controlling mageec, accumulate the arguments which
  // aren't controlling this driver
  std::vector<std::string> command_args;
  for (int i = 0; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg.compare(0, strlen("-fmageec-"), "-fmageec-") != 0) {
      command_args.push_back(arg);
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
    }
    if (handled)
      continue;

    // Flags with values
    if (arg.compare(0, strlen("mode="), "mode=") == 0) {
      std::string mode_str(arg.begin() + strlen("mode="), arg.end());

      if (mode_str == "gather") {
        mode = DriverMode::kGather;
      } else if (mode_str == "optimize") {
        mode = DriverMode::kOptimize;
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
    } else if (arg.compare(0, strlen("config="), "config=") == 0) {
      config_path = std::string(arg.begin() + strlen("config="), arg.end());
      if (config_path.size() == 0) {
        MAGEEC_ERR("No config file path provided");
        return -1;
      }
      with_config = true;
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
  if (mode == DriverMode::kOptimize) {
    if (!with_db) {
      MAGEEC_ERR("Optimize mode specified without a database");
      have_error = true;
    }
    if (!with_config) {
      MAGEEC_ERR("Optimize mode specified without a config file");
      have_error = true;
    }
    if (!with_metric) {
      MAGEEC_ERR("Optimize mode specified without any metric to optimize for");
      have_error = true;
    }
    if (!with_ml) {
      MAGEEC_ERR("Optimize mode specified without a machine learner to use");
      have_error = true;
    }
  } else if (mode == DriverMode::kGather) {
    if (!with_db) {
      MAGEEC_ERR("Gather mode specified without a database");
      have_error = true;
    }
    if (!with_config) {
      MAGEEC_ERR("Gather mode specified without a configuration");
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

  // Initialize the framework, and register some builtin machine learners so
  // they can be selected by UUID by the user.
  mageec::Framework framework(with_debug);

  // C5 classifier
  MAGEEC_DEBUG("Registering C5.0 machine learner interface");
  std::unique_ptr<mageec::IMachineLearner> c5_ml(new mageec::C5Driver());
  framework.registerMachineLearner(std::move(c5_ml));

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
  for (unsigned i = 0; i < command_args.size(); ++i) {
    if (command_args[i] == "-c")
      to_obj = true;

    // Check if the output file has a .o extension
    if (command_args[i] == "-o") {
      ++i;
      if (i < command_args.size()) {
        auto arg = command_args[i];
        if (arg.size() > strlen(".o")) {
          if (arg.compare(arg.size() - strlen(".o"), strlen(".o"), ".o") == 0)
            to_obj = true;
        }
      }
    }
    // A subsequent -S or -E override a preceding -c flag
    if (command_args[i] == "-S" || command_args[i] == "-E") {
      to_obj = false;
    }
  }

  // If we are not in 'gather' or 'optimize' modes, or if we're not compiling
  // to an object file, then just run the original command
  if (!to_obj || (mode == DriverMode::kNone)) {
    std::stringstream command;
    command << command_args[0];
    for (unsigned i = 1; i < command_args.size(); ++i)
      command << " " << command_args[i];

    // FIXME: Windows?
    return system(command.str().c_str());
  }

  // Names of all of the input files involved in the compilation
  std::vector<std::string> input_files;

  // Mapping from an input filename, to a command string to compile that file
  // With the appropriate set of parameters.
  std::map<std::string, std::vector<std::string>> input_file_commands;

  // Mapping source files to feature groups and parameter sets
  std::map<std::string, mageec::ParameterSetID> input_file_params;
  std::map<std::string, mageec::FeatureGroupID> input_file_features;

  // Load the database
  assert((mode == DriverMode::kOptimize) || (mode == DriverMode::kGather));
  std::unique_ptr<mageec::Database> db = framework.getDatabase(db_str, false);
  if (!db) {
    MAGEEC_ERR("Error retrieving database. The database may not exists, or "
               "you may not have sufficient permissions to read it");
    return -1;
  }

  // Load the config file to get the feature groups
  auto feature_groups = loadConfigFile(config_path);
  if (!feature_groups) {
    MAGEEC_ERR("Failed to retrieve feature groups from config file");
    return -1;
  }
  input_file_features = feature_groups.get();

  // Find the input files and strip them from the command line (they will be
  // added back individually later).
  //
  // It is assumed that the input files appear right at the end of the command
  // but this may not always be the case.
  while (command_args.size()) {
    auto arg = command_args.back();

    // Check the extension of the argument
    bool is_input = false;
    is_input |= (arg.size() > strlen(".c")) &&
                !arg.compare(arg.size() - strlen(".c"), strlen(".c"), ".c");
    is_input |= (arg.size() > strlen(".C")) &&
                !arg.compare(arg.size() - strlen(".C"), strlen(".C"), ".C");
    is_input |= (arg.size() > strlen(".s")) &&
                !arg.compare(arg.size() - strlen(".s"), strlen(".s"), ".s");
    is_input |= (arg.size() > strlen(".S")) &&
                !arg.compare(arg.size() - strlen(".S"), strlen(".S"), ".S");

    if (is_input) {
      input_files.push_back(arg);
      command_args.pop_back();
    } else {
      break;
    }
  }

  if (mode == DriverMode::kGather) {
    // Extract the flags that were provided to the compiler on the command
    // line, and build this into a set to be added into the database
    std::set<unsigned> params;

    // First, setup the parameter set according to the basic optimization level
    // The rightmost optimization flag takes precedence
    std::string opt_level = "-O0";
    for (auto arg : command_args) {
      if (arg == "-O0" || arg == "-O"  || arg == "-O1" || arg == "-O2" ||
          arg == "-O3" || arg == "-O4" || arg == "-Os" || arg == "-Ofast") {
        opt_level = arg;
      }
    }
    std::vector<std::string> flags;
    if (opt_level == "-O0")
      flags = opt_flags_O0;
    if (opt_level == "-O" || opt_level == "-O1")
      flags = opt_flags_O1;
    if (opt_level == "-O2")
      flags = opt_flags_O2;
    if (opt_level == "-O3")
      flags = opt_flags_O3;
    if (opt_level == "-O4")
      flags = opt_flags_O4;
    if (opt_level == "-Os")
      flags = opt_flags_Os;
    if (opt_level == "-Ofast")
      flags = opt_flags_Ofast;

    // flag -> parameter
    for (auto f : flags) {
      auto p = flag_to_parameter.find(f);
      if (p != flag_to_parameter.end())
        params.insert(p->second);
    }

    // Handle individual flags. These modify the base set of flags, with the
    // rightmost flag taking precedence.
    //
    // Individual flags may enable the parameter, or disable it if it has a
    // "-fno-" prefix. First check if the flag enables a parameter, then if
    // it disables one.
    for (auto arg : command_args) {
      auto p = flag_to_parameter.find(arg);
      if (p != flag_to_parameter.end()) {
        params.insert(p->second);
      } else {
        if (arg.size() > strlen("-fno-") &&
            !arg.compare(0, strlen("-fno-"), "-fno-")) {
          arg = std::string("-f") +
                std::string(arg.begin() + strlen("-fno-"), arg.end());

          p = flag_to_parameter.find(arg);
          if (p != flag_to_parameter.end()) {
            params.erase(p->second);
          }
        }
      }
    }

    // Turn the set of parameters into a mageec ParameterSet and add it to the
    // database. Use the same parameter set for every file.
    mageec::ParameterSet param_set;
    for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
         i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
      param_set.add(
          std::make_shared<mageec::BoolParameter>(i, params.count(i),
                                                  parameter_to_flag.at(i)));
    }
    mageec::ParameterSetID param_set_id = db->newParameterSet(param_set);
    for (auto file : input_files)
      input_file_params[file] = param_set_id;

    // Build a command to compile each file in turn
    for (auto file : input_files) {
      std::vector<std::string> file_cmd = command_args;
      file_cmd[0] = "gcc"; // FIXME: Don't hardcode in gcc
      file_cmd.push_back(file);

      // Add to the mapping from input files to commands
      input_file_commands[file] = file_cmd;
    }
  } else if (mode == DriverMode::kOptimize) {
    // Strip flags which we can recognize from the command line, build a new
    // command line with those flags omitted
    std::vector<std::string> orig_command_args = command_args;
    std::vector<std::string> new_command_args;
    for (auto arg : command_args) {
      if (arg == "-O0" || arg == "-O"  || arg == "-O1" || arg == "-O2" ||
          arg == "-O3" || arg == "-O4" || arg == "-Os" || arg == "-Ofast") {
        continue;
      }

      if (flag_to_parameter.count(arg))
        continue;

      if (arg.size() > strlen("-fno-") &&
          !arg.compare(0, strlen("-fno-"), "-fno")) {
        std::string tmp = std::string("-f") +
                          std::string(arg.begin() + strlen("-fno-"), arg.end());
        if (flag_to_parameter.count(tmp))
          continue;
      }
      // Not a flag that we need to strip
      new_command_args.push_back(arg);
    }
    command_args = new_command_args;

    // For each input file, use the set of features to generate the flags
    for (auto file : input_files) {
      // If there are no features found for a file, then use the original
      // command with the file appended as an input
      auto feature_group_id = input_file_features.find(file);
      if (feature_group_id == input_file_features.end()) {
        std::vector<std::string> file_cmd = orig_command_args;
        file_cmd.push_back(file);
        input_file_commands[file] = file_cmd;
        continue;
      }

      // Generate a parameter set based on the input features
      std::set<unsigned> params;
      // TODO: Spawn a bunch of decisions and get the machine learner to
      // produce the set of parameters.

      // Turn the set of parameters into a mageec ParameterSet and add it to
      // the database
      mageec::ParameterSet param_set;
      for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
           i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
        param_set.add(
            std::make_shared<mageec::BoolParameter>(i, params.count(i),
                                                    parameter_to_flag.at(i)));
      }
      mageec::ParameterSetID param_set_id = db->newParameterSet(param_set);

      // Build the command line for this file, with the given parameters as
      // its configuration.
      auto cmd_iter = command_args.begin();

      // Add the original command word
      std::vector<std::string> file_cmd;
      file_cmd.push_back(*cmd_iter);
      ++cmd_iter;
      // Add optimization flags
      for (unsigned i = FlagParameterID::kFIRST_FLAG_PARAMETER;
           i <= FlagParameterID::kLAST_FLAG_PARAMETER; ++i) {
        std::string flag = parameter_to_flag.at(i);

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
      for (; cmd_iter != command_args.end(); ++cmd_iter)
        file_cmd.push_back(*cmd_iter);
      // Add the input filename
      file_cmd.push_back(file);

      // Add the mapping from file to parameter set, and from file to command
      input_file_params[file] = param_set_id;
      input_file_commands[file] = file_cmd;
    }
  }

  // Compile each input file in turn, if any fail, error out early
  for (auto file : input_files) {
    auto file_cmd = input_file_commands[file];

    std::stringstream command;
    command << "gcc"; // FIXME: Don't hardcode in gcc
    for (unsigned i = 1; i < file_cmd.size(); ++i)
      command << " " << file_cmd[i];

    // FIXME: Windows?
    MAGEEC_DEBUG("Executing command: " << command.str());
    int res = system(command.str().c_str());
    if (res) {
      MAGEEC_ERR("Compilation failed\ncommand: " << command.str());
      return res;
    }
  }

  // All file compiled successfully, generate compilation ids for them, output
  // these ids into the config file
  std::ofstream config_file(config_path, std::ios::app);
  if (!config_file.is_open()) {
    MAGEEC_ERR("Error opening config file. The file may not exist, or you "
               "may not have sufficient permissions to read and write it");
    return -1;
  }
  for (auto file : input_files) {
    auto feature_group_id = input_file_features.find(file);
    auto parameter_set_id = input_file_params.find(file);

    // If there were no features for this file, then parameters would not have
    // been derived and there will be no compilation id
    if (feature_group_id == input_file_features.end())
      continue;
    assert(parameter_set_id != input_file_params.end());

    // Append the generated compilation ids to the config file
    auto compilation = db->newCompilation(file, "module",
                                          feature_group_id->second,
                                          parameter_set_id->second, nullptr);

    // TODO: Avoid static_cast here
    uint64_t tmp = static_cast<uint64_t>(compilation);
    config_file << file << ",module," << file << ",compilation," << tmp << "\n";
  }
  return 0;
}