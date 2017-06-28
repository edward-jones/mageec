#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys

import mageec
from mageec import eprint


def build(src_dir, build_dir, install_dir, cc, cxx, fort, build_flags):
    assert (mageec.is_command_on_path('cmake'))
    assert (mageec.is_command_on_path('make'))

    base_dir = os.getcwd()
    with mageec.preserve_cwd():
        os.chdir(build_dir)
        cmd = ['cmake', src_dir, '-G', 'Unix Makefiles']
        cmd.append('-DCMAKE_C_COMPILER=' + cc)
        cmd.append('-DCMAKE_CXX_COMPILER=' + cxx)
        if build_flags != '':
            cmd.append('-DCMAKE_C_FLAGS=' + build_flags)
            cmd.append('-DCMAKE_CXX_FLAGS=' + build_flags)
        cmd.append('-DCMAKE_INSTALL_PREFIX=' + install_dir)

        # configure the build
        print (' '.join(cmd))
        ret = subprocess.call(cmd)
        if ret != 0:
            eprint ('-- Failed to configure \'' + src_dir + '\' using CMake')
            return False

        # Call make to do the build
        cmd = ['make']
        print (' '.join(cmd))
        ret = subprocess.call(cmd)
        if ret != 0:
            eprint ('-- Failed to build source \'' + src_dir + '\' using CMake')
            return False

        cmd = ['make', 'install']
        print (' '.join(cmd))
        ret = subprocess.call(cmd)
        if ret != 0:
            eprint ('-- Failed to install build \'' + build_dir + '\' to \'' + install_dir)
            return False
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Build a project using CMake and Makefiles')

    # required arguments
    parser.add_argument('--src-dir', nargs=1, required=True,
        help='Directory containing the source to build')
    parser.add_argument('--build-dir', nargs=1, required=True,
        help='Directory to hold the build files')
    parser.add_argument('--install-dir', nargs=1, required=True,
        help='Directory to hold the installed files')
    parser.add_argument('--cc', nargs=1, required=True,
        help='Command to use to compile C source')
    parser.add_argument('--cxx', nargs=1, required=True,
        help='Command to use to compile C++ source')
    parser.add_argument('--fort', nargs=1, required=True,
        help='Command to use to compile Fortran source')

    # optional arguments
    parser.add_argument('--build-flags', nargs=1, required=True,
        help='Argument to be used in all compilations')
    parser.set_defaults(build_flags=[''])

    args = parser.parse_args(sys.argv[1:])
    src_dir         = os.path.abspath(args.src_dir[0])
    build_dir       = os.path.abspath(args.build_dir[0])
    install_dir     = os.path.abspath(args.install_dir[0])
    cc              = args.cc[0]
    cxx             = args.cxx[0]
    fort            = args.fort[0]

    build_flags = args.build_flags[0]

    if not os.path.exists(src_dir):
        eprint ('-- Source directory \'' + src_dir + '\' does not exist')
        return -1
    if not os.path.exists(build_dir):
        eprint ('-- Build directory \'' + build_dir + '\' does not exist')
        return -1
    if not os.path.exists(install_dir):
        eprint ('-- Install directory \'' + install_dir + '\' does not exist')
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

    res = build(src_dir, build_dir, install_dir, cc, cxx, fort, build_flags)
    if not res:
        return -1
    return 0


if __name__ == '__main__':
    main()

