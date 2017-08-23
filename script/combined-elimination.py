#!/usr/bin/env python3

import argparse
import os
import shutil
import stat
import subprocess
import sys

import mageec
from mageec import eprint

from multiprocessing.pool import Pool


# These are the various flags that can be controlled, as well as the
# first version of GCC that they were supported in
flag_version = {
    '-faggressive-loop-optimizations':      40800,
    '-falign-functions':                    40500,
    '-falign-jumps':                        40500,
    '-falign-labels':                       40500,
    '-falign-loops':                        40500,
    '-fbranch-count-reg':                   40500,
    '-fbranch-target-load-optimize':        40500,
    #'-fbranch-target-load-optimize2':       40500,  # Can't run multiple times
    '-fbtr-bb-exclusive':                   40500,
    '-fcaller-saves':                       40500,
    '-fcombine-stack-adjustments':          40600,
    #'-fcommon':                             40500,  # affects semantics, unlikely to affect performance
    '-fcompare-elim':                       40600,
    '-fconserve-stack':                     40500,
    '-fcprop-registers':                    40500,
    '-fcrossjumping':                       40500,
    '-fcse-follow-jumps':                   40500,
    #'-fdata-sections':                      40500,  # affects semantics, unlikely to affect performance
    '-fdce':                                40500,
    '-fdefer-pop':                          40500,
    '-fdelete-null-pointer-checks':         40500,
    '-fdevirtualize':                       40600,
    '-fdse':                                40500,
    '-fearly-inlining':                     40500,
    '-fexpensive-optimizations':            40500,
    '-fforward-propagate':                  40500,
    '-fgcse':                               40500,
    '-fgcse-after-reload':                  40500,
    '-fgcse-las':                           40500,
    '-fgcse-lm':                            40500,
    '-fgcse-sm':                            40500,
    '-fguess-branch-probability':           40500,
    '-fhoist-adjacent-loads':               40800,
    '-fif-conversion':                      40500,
    '-fif-conversion2':                     40500,
    '-finline':                             40500,
    '-finline-atomics':                     40700,
    '-finline-functions':                   40500,
    '-finline-functions-called-once':       40500,
    '-finline-small-functions':             40500,
    '-fipa-cp':                             40500,
    '-fipa-cp-clone':                       40500,
    '-fipa-profile':                        40600,
    '-fipa-pta':                            40500,
    '-fipa-pure-const':                     40500,
    '-fipa-reference':                      40500,
    '-fipa-sra':                            40500,
    '-fira-hoist-pressure':                 40800,
    '-fivopts':                             40500,
    '-fmerge-constants':                    40500,
    '-fmodulo-sched':                       40500,
    '-fmove-loop-invariants':               40500,
    '-fomit-frame-pointer':                 40500,
    '-foptimize-sibling-calls':             40500,
    '-foptimize-strlen':                    40700,
    '-fpeephole':                           40500,
    '-fpeephole2':                          40500,
    '-fpredictive-commoning':               40500,
    '-fprefetch-loop-arrays':               40500,
    '-fregmove':                            40500,
    '-frename-registers':                   40500,
    '-freorder-blocks':                     40500,
    '-freorder-functions':                  40500,
    '-frerun-cse-after-loop':               40500,
    '-freschedule-modulo-scheduled-loops':  40500,
    '-fsched-critical-path-heuristic':      40500,
    '-fsched-dep-count-heuristic':          40500,
    '-fsched-group-heuristic':              40500,
    '-fsched-interblock':                   40500,
    '-fsched-last-insn-heuristic':          40500,
    '-fsched-pressure':                     40500,
    '-fsched-rank-heuristic':               40500,
    '-fsched-spec':                         40500,
    '-fsched-spec-insn-heuristic':          40500,
    '-fsched-spec-load':                    40500,
    '-fsched-stalled-insns':                40500,
    '-fsched-stalled-insns-dep':            40500,
    '-fschedule-insns':                     40500,
    '-fschedule-insns2':                    40500,
    #'-fsection-anchors':                    40500,  # may conflict with other flags
    '-fsel-sched-pipelining':               40500,
    '-fsel-sched-pipelining-outer-loops':   40500,
    '-fsel-sched-reschedule-pipelined':     40500,
    '-fselective-scheduling':               40500,
    '-fselective-scheduling2':              40500,
    '-fshrink-wrap':                        40700,
    '-fsplit-ivs-in-unroller':              40500,
    '-fsplit-wide-types':                   40500,
    #'-fstrict-aliasing':                    40500,  # affects semantics
    '-fthread-jumps':                       40500,
    '-ftoplevel-reorder':                   40500,
    '-ftree-bit-ccp':                       40600,
    '-ftree-builtin-call-dce':              40500,
    '-ftree-ccp':                           40500,
    '-ftree-ch':                            40500,
    #'-ftree-coalesce-inlined-vars':         40500,  # No equivalent -fno for this flag
    '-ftree-coalesce-vars':                 40800,
    '-ftree-copy-prop':                     40500,
    '-ftree-copyrename':                    40500,
    '-ftree-cselim':                        40500,
    '-ftree-dce':                           40500,
    '-ftree-dominator-opts':                40500,
    '-ftree-dse':                           40500,
    '-ftree-forwprop':                      40500,
    '-ftree-fre':                           40500,
    '-ftree-loop-distribute-patterns':      40600,
    '-ftree-loop-distribution':             40500,
    '-ftree-loop-if-convert':               40600,
    '-ftree-loop-im':                       40500,
    '-ftree-loop-ivcanon':                  40500,
    '-ftree-loop-optimize':                 40500,
    '-ftree-partial-pre':                   40800,
    '-ftree-phiprop':                       40500,
    '-ftree-pre':                           40500,
    '-ftree-pta':                           40500,
    '-ftree-reassoc':                       40500,
    '-ftree-scev-cprop':                    40500,
    '-ftree-sink':                          40500,
    '-ftree-slp-vectorize':                 40500,
    '-ftree-slsr':                          40800,
    '-ftree-sra':                           40500,
    '-ftree-switch-conversion':             40500,
    '-ftree-tail-merge':                    40700,
    '-ftree-ter':                           40500,
    '-ftree-vect-loop-version':             40500,
    '-ftree-vectorize':                     40500,
    '-ftree-vrp':                           40500,
    '-funroll-all-loops':                   40500,
    '-funroll-loops':                       40500,
    '-funswitch-loops':                     40500,
    '-fvariable-expansion-in-unroller':     40500,
    '-fvect-cost-model':                    40500,
    '-fweb':                                40500,
}


def build_and_measure(src_dir, run_dir, cc, cxx, fort,
                      database, features, build_script, measure_script,
                      build_flags, exec_flags, debug):
    assert(os.path.exists(src_dir))
    assert(os.path.exists(database))
    assert(os.path.exists(features))

    build_dir   = os.path.join(run_dir, 'build')
    install_dir = os.path.join(run_dir, 'install')
    assert(not os.path.exists(build_dir))
    assert(not os.path.exists(install_dir))
    os.makedirs(build_dir)
    os.makedirs(install_dir)

    # build the benchmark using the wrapper driver
    # This will record the flags which the benchmark is being compiled with,
    # and associate these flags with previously extracted features through
    # an integer identifier of the compilation in the database
    compilations = os.path.join(install_dir, 'compilations.csv')
    results_path = os.path.join(install_dir, 'results.csv')

    cc_wrapper   = 'mageec-' + cc
    cxx_wrapper  = 'mageec-' + cxx
    fort_wrapper = 'mageec-' + fort
    assert(mageec.is_command_on_path(cc_wrapper))
    assert(mageec.is_command_on_path(cxx_wrapper))
    assert(mageec.is_command_on_path(fort_wrapper))
    wrapper_flags = ''
    if debug:
        wrapper_flags += ' -fmageec-debug'
    wrapper_flags += ' -fmageec-mode=gather'
    wrapper_flags += ' -fmageec-database=' + database
    wrapper_flags += ' -fmageec-features=' + features
    wrapper_flags += ' -fmageec-out=' + compilations

    # add the wrapper flags to the front of the flags and then build using
    # the provided build script
    new_flags = wrapper_flags
    if build_flags != '':
        new_flags += ' ' + ' '.join(build_flags)
    res = mageec.build(src_dir=src_dir,
                       build_dir=build_dir,
                       install_dir=install_dir,
                       build_script=build_script,
                       cc=cc_wrapper,
                       cxx=cxx_wrapper,
                       fort=fort_wrapper,
                       build_flags=new_flags)
    if not res:
        eprint ('-- Failed to build source \'' + src_dir + '\'')
        return 0

    # Find the measure script. If it's not on the path, look in the
    # directory this script is in. If it's not in that directory then look
    # in the current working directory
    if mageec.is_command_on_path(measure_script):
        cmd_path = measure_script
    else:
        found_script = False
        if not os.path.isabs(measure_script):
            # Check if it's a script in the same directory as this script
            script_dir = os.path.dirname(os.path.realpath(__file__))
            cmd_path = os.path.join(script_dir, measure_script)
            if os.path.exists(cmd_path):
                found_script = True
        if not found_script:
            cmd_path = os.path.abspath(measure_script)
            if not os.path.exists(cmd_path):
                eprint ('-- Failed to find script to measure benchmark result')
                return 0

    # run the measurement script on each of the executables in the
    # install directory to produce a results file
    exec_files = []
    exec_bits = stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH
    for d in os.walk(install_dir):
        dir_path, _, files = d
        for f in files:
            f = os.path.join(dir_path, f)
            if os.path.isfile(f):
                stat_res = os.stat(f)
                stat_mode = stat_res.st_mode
                if stat_mode & exec_bits:
                    exec_files.append(f) 

    total = 0
    for exec_path in exec_files:
        cmd = [cmd_path,
               '--exec-path', exec_path,
               '--compilation-ids', compilations,
               '--out', results_path]
        if exec_flags != '':
            cmd.append('--exec-flags')
            cmd.append(' '.join(exec_flags))
        print (' '.join(cmd))
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
        result_value = float(p.communicate()[0].decode().strip())
        if p.returncode != 0:
            eprint ('-- Failed to measure executable \'' + exec_path + '\'')
            return 0
        else:
            total += result_value
    return total


def combined_elimination(src_dir, run_dir, cc, cxx, fort,
                         database, features, build_script, measure_script,
                         base_opt, build_flags, exec_flags, jobs, debug):
    assert(os.path.exists(src_dir) and os.path.isabs(src_dir))
    assert(os.path.exists(run_dir) and os.path.isabs(run_dir))
    assert(mageec.is_command_on_path(cc))
    assert(mageec.is_command_on_path(cxx))
    assert(mageec.is_command_on_path(fort))
    assert(os.path.exists(database))
    assert(os.path.exists(features))
    assert(jobs > 0)

    # Get the version of the compiler and determine which flags are available
    # for tuning
    cmd = [cc, '-dumpversion']
    print (' '.join(cmd))
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    gcc_major, gcc_minor, gcc_patch = p.communicate()[0].decode().strip().split('.')
    gcc_version = (int(gcc_major) * 10000) +  (int(gcc_minor) * 100) + int(gcc_patch)

    all_flags = []
    for flag in flag_version:
        version = flag_version[flag]
        if version <= gcc_version:
            all_flags.append(flag)

    # Flags to consider as candidates to be disabled. This list will shrink
    # when flags are disabled permanently
    flags_to_consider = list(all_flags)

    # Build at the base optimization level. This baseline build isn't actually
    # run, but can be used as a point of comparison later.
    assert(base_opt.strip() in ['-O3', '-Os'])

    base_build_dir   = os.path.join(run_dir, 'base', 'build')
    base_install_dir = os.path.join(run_dir, 'base', 'install')
    assert(not os.path.exists(base_build_dir))
    assert(not os.path.exists(base_install_dir))
    os.makedirs(base_build_dir)
    os.makedirs(base_install_dir)

    res = mageec.build(src_dir=src_dir,
                       build_dir=base_build_dir,
                       install_dir=base_install_dir,
                       build_script=build_script,
                       cc=cc,
                       cxx=cxx,
                       fort=fort,
                       build_flags=build_flags + ' ' + base_opt)
    if not res:
        eprint ('-- Failed baseline build')
        return False

    # Identifier for the current run
    run_id = 0

    # Begin with all flags enabled
    curr_flags = list(all_flags)
    curr_result = build_and_measure(src_dir,
                                    os.path.join(run_dir, 'test.' + str(run_id)),
                                    cc, cxx, fort,
                                    database,
                                    features,
                                    build_script,
                                    measure_script,
                                    [build_flags, base_opt] + curr_flags,
                                    [exec_flags],
                                    debug)
    if curr_result <= 0:
        eprint ('-- Failed initial test run')
        return False
    print ('CE: (best) id: ' + str(run_id) + ' result: ' + str(curr_result) + ' flags: ' + ' '.join(curr_flags))
    run_id += 1

    finished = False
    while not finished:
        finished = True

        # identify flags that bring improvement when they are disabled
        # individually. Do all of these builds in parallel
        flags_with_improvement = []
        test_pool = Pool(jobs)
        test_runs = []
        for test_flag in flags_to_consider:
            run_flags = list(curr_flags)

            # Flip the flag to have a -fno- prefix
            assert(test_flag in run_flags)
            flag_index = run_flags.index(test_flag)
            run_flags[flag_index] = '-fno-' + test_flag[2:]

            # Build and measure with this flag disabled
            run_proc = test_pool.apply_async(build_and_measure,
                                             (src_dir,
                                              os.path.join(run_dir, 'test.' + str(run_id)),
                                              cc, cxx, fort,
                                              database,
                                              features,
                                              build_script,
                                              measure_script,
                                              [build_flags, base_opt] + run_flags,
                                              [exec_flags],
                                              debug))
            test_runs.append((run_proc, run_id, test_flag))
            run_id += 1

        # Wait for all of the test runs to complete, and then get their results
        for run_proc, _, _ in test_runs:
            run_proc.wait()
        for run_proc, test_run_id, test_flag in test_runs:
            run_result = run_proc.get()
            if run_result <= 0:
                eprint ('-- Ignoring failed test run ' + str(test_run_id))
                continue
            print ('CE: (test) id: ' + str(test_run_id) + ' result: ' + str(run_result) + ' flag: ' + test_flag)
            # Consider a flag as being an improvement even if it's up to 1%
            # worse than the current base result. This avoids small
            # measurement errors from disrupting the process.
            if run_result < (curr_result * 1.01):
                flags_with_improvement.append((test_flag, test_run_id, run_result))

        # Starting from the flag which had the biggest improvement, check
        # if disabling the flag still gives a better result. If it does, then
        # permanently disable the flag and update the base flag
        # configuration.
        #
        # The first flag is guaranteed to give an improvement, so we don't
        # need to re-run the test
        flags_with_improvement = sorted(flags_with_improvement, key=lambda x: x[2])
        if len(flags_with_improvement) == 0:
            break

        # If the first flag really is better than the current best, then
        # immediately invert it and remove it from further consideration
        test_flag, test_run_id, run_result = flags_with_improvement[0]
        if run_result < curr_result:
            assert test_flag in curr_flags
            flag_index = curr_flags.index(test_flag)
            curr_flags[flag_index] = '-fno-' + test_flag[2:]
            curr_result = run_result
            print ('CE: (best) id: ' + str(test_run_id) + ' result: ' + str(curr_result) + ' flags: ' + ' '.join(curr_flags))
            flags_to_consider.remove(test_flag)
            flags_with_improvement = flags_with_improvement[1:]

        # Keep disabling flags which previously gave an improvement
        # in turn until we stop seeing an improvement
        for flag, _, _ in flags_with_improvement:
            run_flags = list(curr_flags)

            # Flip the flag to have a -fno- prefix
            assert flag in run_flags
            flag_index = run_flags.index(flag)
            run_flags[flag_index] = '-fno-' + flag[2:]

            # Run with this flag disabled as well
            test_run_id = run_id
            run_result = build_and_measure(src_dir,
                                           os.path.join(run_dir, 'test.' + str(run_id)),
                                           cc, cxx, fort,
                                           database,
                                           features,
                                           build_script,
                                           measure_script,
                                           [build_flags, base_opt] + run_flags,
                                           [exec_flags],
                                           debug)
            print ('CE: (test) id: ' + str(test_run_id) + ' result: ' + str(run_result) + ' flag: ' + flag)
            run_id += 1

            if run_result <= 0:
                eprint ('-- Ignoring failed test run ' + str(test_run_id))
            # TODO: If this flag is within 1% of the current result, then
            # rerun a couple more times in case it's just a measurement
            # anomaly
            if run_result < curr_result:
                # Disabling this flag was an improvement, so permanently
                # disable it and remove it from further consideration
                curr_flags = run_flags
                curr_result = run_result
                print ('CE: (best) id: ' + str(test_run_id) + ' result: ' + str(curr_result) + ' flags: ' + ' '.join(curr_flags))
                flags_to_consider.remove(flag)

                # Another iteration of combined elimination is necessary
                finished = False


def main():
    parser = argparse.ArgumentParser(
        description='Generate and build multiple versions of a source project')

    # required arguments
    parser.add_argument('--src-dir', nargs=1, required=True,
        help='Directory containing the source to build')
    parser.add_argument('--run-dir', nargs=1, required=True,
        help='Directory to hold each combined elimination run')
    parser.add_argument('--cc', nargs=1, required=True,
        help='Command to use to compile C source')
    parser.add_argument('--cxx', nargs=1, required=True,
        help='Command to use to compile C++ source')
    parser.add_argument('--fort', nargs=1, required=True,
        help='Command to use to compile Fortran source')
    parser.add_argument('--database', nargs=1, required=True,
        help='mageec database to store generated compilations in')
    parser.add_argument('--features', nargs=1, required=True,
        help='File containing extracted features for the source being built')
    parser.add_argument('--base-opt', nargs=1, required=True,
        help='Base optimization level to use as a starting point. '
             'Valid values are \'-O3\' and \'-Os\'')
    parser.add_argument('--build-script', nargs=1, required=True,
        help='Script to build the benchmarks')
    parser.add_argument('--measure-script', nargs=1, required=True,
        help='Script to measure the resultant executables')

    # optional arguments
    parser.add_argument('--build-flags', nargs=1, required=False,
        help='Argument to be used in all compilations')
    parser.add_argument('--exec-flags', nargs=1, required=False,
        help='Flags to use when executing generated programs')
    parser.add_argument('--jobs', nargs=1, required=False,
        help='Number of jobs to run when building')
    parser.add_argument('--debug', action='store_true', required=False,
        help='Enable debug when doing feature extraction')
    parser.set_defaults(build_flags=[''],
                        exec_flags=[''],
                        jobs=[1],
                        debug=False)

    args = parser.parse_args(sys.argv[1:])
    src_dir         = os.path.abspath(args.src_dir[0])
    run_dir         = os.path.abspath(args.run_dir[0])
    cc              = args.cc[0]
    cxx             = args.cxx[0]
    fort            = args.fort[0]
    database        = os.path.abspath(args.database[0])
    features        = os.path.abspath(args.features[0])
    base_opt        = args.base_opt[0]
    build_script    = args.build_script[0]
    measure_script  = args.measure_script[0]

    build_flags = args.build_flags[0]
    exec_flags  = args.exec_flags[0]
    jobs        = int(args.jobs[0])
    debug       = args.debug

    if not os.path.exists(src_dir):
        eprint ('-- Source directory \'' + src_dir + '\' does not exist')
        return -1
    if not os.path.exists(database):
        eprint ('-- Database \'' + database + '\' does not exist')
        return -1
    if not os.path.exists(features):
        eprint ('-- Features file \'' + features + '\' does not exist')
        return -1

    if not mageec.is_command_on_path(cc):
        eprint ('-- Compiler \'' + cc + '\' is not on the path')
        return -1
    if not mageec.is_command_on_path(cxx):
        eprint ('-- Compiler \'' + cxx + '\' is not on the path')
        return -1
    if not mageec.is_command_on_path(fort):
        eprint ('-- Compiler \'' + fort + '\' is not on the path')
        return -1

    res = combined_elimination(src_dir=src_dir,
                               run_dir=run_dir,
                               cc=cc,
                               cxx=cxx,
                               fort=fort,
                               database=database,
                               features=features,
                               build_script=build_script,
                               measure_script=measure_script,
                               base_opt=base_opt,
                               build_flags=build_flags,
                               exec_flags=exec_flags,
                               jobs=jobs,
                               debug=debug)
    if not res:
        return -1
    return 0


if __name__ == '__main__':
    main()

