#!/usr/bin/env python3

import os
import subprocess
import sys


def get_gcc_plugin_name():
    return 'libgcc_feature_extract'


def is_command_on_path(cmd):
    for path in os.environ['PATH'].split(os.pathsep):
        path = path.strip('"')
        exec_file = os.path.join(path, cmd)
        if os.path.isfile(exec_file) and os.access(exec_file, os.X_OK):
            return True
    return False


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


class preserve_cwd():
    def __enter__(self):
        self.cwd = os.getcwd()
    def __exit__(self, type, value, traceback):
        os.chdir(self.cwd)


def build(src_dir, build_dir, install_dir, build_script, cc, cxx, fort,
          build_flags):
    assert(os.path.exists(src_dir) and os.path.isabs(src_dir))
    assert(os.path.exists(build_dir) and os.path.isabs(build_dir))
    assert(os.path.exists(install_dir) and os.path.isabs(install_dir))
    assert(is_command_on_path(cc))
    assert(is_command_on_path(cxx))
    assert(is_command_on_path(fort))

    print ('-- Building source    \'' + src_dir + '\'')
    print ('   Build directory:   \'' + build_dir + '\'')
    print ('   Install directory: \'' + install_dir + '\'')
    print ('   Build script:      \'' + build_script + '\'')

    # Find the build script. If it's not on the path, look in the directory
    # this script is in. If it's not in that directory then look in the
    # current working directory
    if is_command_on_path(build_script):
        cmd_path = build_script
    else:
        found_script = False
        if not os.path.isabs(build_script):
            # Check if it's a script in the same directory as this script
            script_dir = os.path.dirname(os.path.realpath(__file__))
            cmd_path = os.path.join(script_dir, build_script)
            if os.path.exists(cmd_path):
                found_script = True
        if not found_script:
            cmd_path = os.path.abspath(build_script)
            if not os.path.exists(cmd_path):
                eprint ('-- Failed to find script to build source \'' + src_dir + '\'')
                return False

    cmd = [cmd_path,
           '--src-dir', src_dir,
           '--build-dir', build_dir,
           '--install-dir', install_dir,
           '--cc', cc,
           '--cxx', cxx,
           '--fort', fort,
           '--build-flags', build_flags]
    print (' '.join(cmd))
    ret = subprocess.call(cmd)
    if ret != 0:
        eprint ('-- Failed to build using custom build script \'' + build_script + '\'')
        return False
    return True


def feature_extract(src_dir, build_dir, install_dir, cc, cxx, fort,
                    database, build_script, build_flags, gcc_plugin_path,
                    debug, out):
    assert(os.path.exists(src_dir) and os.path.isabs(src_dir))
    assert(os.path.exists(build_dir) and os.path.isabs(build_dir))
    assert(os.path.exists(install_dir) and os.path.isabs(install_dir))
    assert(is_command_on_path(cc))
    assert(is_command_on_path(cxx))
    assert(is_command_on_path(fort))
    assert(os.path.exists(database))
    assert(os.path.exists(gcc_plugin_path))
    
    gcc_plugin_name = get_gcc_plugin_name()
    plugin_flags = '-fplugin=' + gcc_plugin_path
    if debug:
        plugin_flags += ' -fplugin-arg-' + gcc_plugin_name + '-debug'
    plugin_flags += ' -fplugin-arg-' + gcc_plugin_name + '-database=' + database
    plugin_flags += ' -fplugin-arg-' + gcc_plugin_name + '-out=' + out

    new_flags = plugin_flags
    if build_flags != '':
        new_flags += build_flags

    print ('-- Performing feature extraction')
    return build(src_dir=src_dir,
                 build_dir=build_dir,
                 install_dir=install_dir,
                 build_script=build_script,
                 cc=cc,
                 cxx=cxx,
                 fort=fort,
                 build_flags=new_flags)

