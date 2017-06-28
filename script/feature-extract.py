#!/usr/bin/env python3

import argparse
import os
import sys

import mageec
from mageec import eprint


def main():
    parser = argparse.ArgumentParser(
        description='Perform feature extraction')

    # required arguments
    parser.add_argument('--src-dir', nargs=1, required=True,
        help='Directory containing the source to build')
    parser.add_argument('--build-dir', nargs=1, required=True,
        help='Build directory')
    parser.add_argument('--install-dir', nargs=1, required=True,
        help='Install directory')
    parser.add_argument('--cc', nargs=1, required=True,
        help='Command to use to compile C source')
    parser.add_argument('--cxx', nargs=1, required=True,
        help='Command to use to compile C++ source')
    parser.add_argument('--fort', nargs=1, required=True,
        help='Command to use to compile Fortan source')
    parser.add_argument('--database', nargs=1, required=True,
        help='mageec database to store extracted features into')
    parser.add_argument('--mageec-lib-path', nargs=1, required=True,
        help='Path to the directory holding the mageec libraries')
    parser.add_argument('--build-script', nargs=1, required=True,
        help='Script to build the benchmarks')
    parser.add_argument('--out', nargs=1, required=True,
        help='File to output the extracted features to')

    # optional arguments
    parser.add_argument('--debug', action='store_true', required=False,
        help='Enable debug when doing feature extraction')
    parser.add_argument('--build-flags', nargs=1, required=False,
        help='Common arguments to be used when building')
    parser.set_defaults(debug=False,
                        build_flags=[''])

    args = parser.parse_args(sys.argv[1:])
    src_dir         = os.path.abspath(args.src_dir[0])
    build_dir       = os.path.abspath(args.build_dir[0])
    install_dir     = os.path.abspath(args.install_dir[0])
    cc              = args.cc[0]
    cxx             = args.cxx[0]
    fort            = args.fort[0]
    database        = os.path.abspath(args.database[0])
    mageec_lib_path = os.path.abspath(args.mageec_lib_path[0])
    build_script    = args.build_script[0]
    out             = os.path.abspath(args.out[0])

    debug       = args.debug
    build_flags = args.build_flags[0]

    # TODO: Extension should depend on platform
    gcc_plugin_name = mageec.get_gcc_plugin_name() + '.so'

    if not os.path.exists(src_dir):
        eprint ('-- Source directory \'' + src_dir + '\' does not exist')
        return -1
    if not os.path.exists(database):
        eprint ('-- Database \'' + database + '\' does not exist')
        return -1

    if os.path.exists(build_dir):
        eprint ('-- Build directory \'' + build_dir + '\' already exists')
        return -1
    if os.path.exists(install_dir):
        eprint ('-- Install directory \'' + install_dir + '\' already exists')
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

    print ('-- Checking for mageec plugin \'' + gcc_plugin_name + '\'')
    gcc_plugin_path = os.path.join(mageec_lib_path, gcc_plugin_name)
    if not os.path.exists(gcc_plugin_path):
        eprint ('-- Could not find gcc plugin')
        return -1

    os.makedirs(build_dir)
    os.makedirs(install_dir)

    res = mageec.feature_extract(src_dir=src_dir,
                                 build_dir=build_dir,
                                 install_dir=install_dir,
                                 cc=cc,
                                 cxx=cxx,
                                 fort=fort,
                                 database=database,
                                 build_script=build_script,
                                 build_flags=build_flags,
                                 gcc_plugin_path=gcc_plugin_path,
                                 debug=debug,
                                 out=out)
    if not res:
        return -1
    return 0


if __name__ == '__main__':
    sys.exit(main())
