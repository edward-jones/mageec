#!/usr/bin/env python3

import argparse
import os
import stat
import subprocess
import sys

import mageec
from mageec import eprint

from elftools.common.py3compat import itervalues
from elftools.elf.elffile import ELFFile
from elftools.elf.elffile import SymbolTableSection


def read_compilation_ids(compilation_id_path):
    src_module = {}
    src_functions = {}
    src_compilations = {}

    for line in open(compilation_id_path):
        values = line.split(',')

        if len(values) != 5:
            continue
        if values[3] != 'compilation':
            continue

        src_path = values[0]
        if not os.path.exists(src_path):
            eprint ('-- Compilation id csv file references non-existent file \'' + src_path + '\'. Ignoring...')
            continue

        entry_type = values[1]
        if entry_type == 'module':
            module_name = values[2].strip()
            compilation_id = values[4].strip()
            if src_path in src_module:
                eprint ('-- Multiple module compilation ids for source file \'' + src_path + '\'. Ignoring...')
                continue
            src_module[src_path] = (module_name, compilation_id)
        elif entry_type == 'function':
            func_name = values[2].strip()
            compilation_id = values[4].strip()
            if src_path in src_functions and func_name in src_functions[src_path]:
                eprint ('-- Multiple compilation ids for function \'' + func_name + '\' in \'' + src_path + '\'. Ignoring...')
                continue
            if src_path not in src_functions:
                src_functions[src_path] = {}
            src_functions[src_path][func_name] = compilation_id
        else:
            eprint ('-- Unknown entry in compilation id csv file\n'
                    '   ' + line)

    src_paths = set(list(src_module.keys()) + list(src_functions.keys()))
    for src_path in src_paths:
        module = None
        if src_path in src_module:
            module = src_module[src_path]

        funcs = []
        if src_path in src_functions:
            func_names = src_functions[src_path].keys()
            for func_name in func_names:
                funcs.append((func_name, src_functions[src_path][func_name]))

        src_compilations[src_path] = (module, funcs)
    return src_compilations


def get_executable_src_files(exec_path):
    assert (os.path.isabs(exec_path))

    exec_src_paths = []
    with open(exec_path, 'rb') as elf_file_handle:
        try:
            elf_file = ELFFile(elf_file_handle)
        except:
            eprint ('-- Executable \'' + exec_path + '\' is not an ELF file')
            return []

        if not elf_file.has_dwarf_info():
            eprint ('-- Executable \'' + exec_path + '\' has no DWARF information')
            return []

        dwarf_info = elf_file.get_dwarf_info()
        for CU in dwarf_info.iter_CUs():
            DIE = CU.get_top_DIE()

            name = ''
            comp_dir = ''
            for attr in itervalues(DIE.attributes):
                if attr.name == 'DW_AT_name':
                    name = attr.value
                if attr.name == 'DW_AT_comp_dir':
                    comp_dir = attr.value

            # If the source path in the executable is not an absolute
            # path then use the DW_AT_comp_dir attribute to get the
            # build directory to make it absolute
            #
            # Once we have an absolute path, use realpath to resolve any
            # symbolic links
            src_path = name
            if not os.path.isabs(name):
                assert (comp_dir != '')
                src_path = os.path.join(comp_dir, name)
                assert(os.path.isabs(src_path))
                src_path = os.path.realpath(src_path)
            exec_src_paths.append(src_path.decode())
    return exec_src_paths


def get_executable_symbol_sizes(exec_path):
    assert (os.path.isabs(exec_path))

    sym_sizes = {}
    with open(exec_path, 'rb') as elf_file_handle:
        try:
            elf_file = ELFFile(elf_file_handle)
        except:
            eprint ('-- Executable \'' + exec_path + '\' is not an ELF file')
            return {}

        for section in elf_file.iter_sections():
            if not isinstance(section, SymbolTableSection):
                continue

            for _, symbol in enumerate(section.iter_symbols()):
                sym_sizes[symbol.name] = symbol['st_size']
    return sym_sizes


def measure_executable(exec_path, compilation_ids, debug, out):
    assert(os.path.exists(exec_path) and os.path.isabs(exec_path))
    assert(os.path.exists(compilation_ids) and os.path.isabs(compilation_ids))
    assert(os.path.isabs(out))

    # Dictionary mapping from source files to module and function compilations
    # ids
    src_file_compilation_ids = read_compilation_ids(compilation_ids)

    # List of source files which make up the executable
    exec_src_files = get_executable_src_files(exec_path)

    # Dictionary of all of the symbols and their sizes in the executable
    symbol_sizes = get_executable_symbol_sizes(exec_path)

    # For each file referenced in the executable, look for associated
    # compilation ids
    compilation_sizes = {}
    compilation_metadata = {}

    total_size = 0
    for exec_src_file in exec_src_files:
        if not os.path.exists(exec_src_file):
            eprint ('-- File referenced in executable \'' + exec_src_file + '\' '
                    'does not exist')
            continue
        if exec_src_file not in src_file_compilation_ids:
            eprint ('-- File referenced in executable, but has no associated '
                    'compilation ids: \'' + exec_src_file + '\'... Ignoring')
            continue

        module = src_file_compilation_ids[exec_src_file][0]
        funcs = src_file_compilation_ids[exec_src_file][1]

        # Try and find symbols for each function with a compilation id.
        module_size = 0
        for func_name, func_id in funcs:
            if func_name in symbol_sizes:
                func_size = symbol_sizes[func_name]

                if func_size == 0:
                    eprint ('-- Size of function \'' + func_name + '\' is 0... Ignoring')
                else:
                    total_size += func_size

                    # Store metadata about the function
                    compilation_metadata[func_id] = (exec_src_file, 'function', func_name)

                    # Store the compilation id and its size
                    if not func_id in compilation_sizes:
                        compilation_sizes[func_id] = [func_size]
                    else:
                        compilation_sizes[func_id].append(func_size)

                # Accumulate module size too
                module_size += func_size
            else:
                # We have a function with a compilation id, but it doesn't
                # appear in the executable. This probably implies that it
                # has been inlined.
                eprint ('-- Function \'' + func_name + '\' appeared in '
                        'a compilation but not in the final executable')

        # Now we have a module size too, so we can store it into the results
        if module:
            module_name = module[0]
            if module_size == 0:
                eprint ('-- Size of module \'' + module_name + '\' is 0... Ignoring')
            else:
                _, module_id = module

                # Store metadata about the module
                compilation_metadata[module_id] = (exec_src_file, 'module', module_name)

                if not module_id in compilation_sizes:
                    compilation_sizes[module_id] = [module_size]
                else:
                    compilation_sizes[module_id].append(module_size)

    with open(out, 'a') as out_file:
        for compilation_id in compilation_sizes:
            sizes = compilation_sizes[compilation_id]
            metadata = compilation_metadata[compilation_id]

            src_file = metadata[0]
            compilation_type = metadata[1]
            compilation_name = metadata[2]

            # It does not make much sense to have multiple results for the
            # same compilation id. We emit duplicate results into the
            # file anyway, and let mageec deal with the problem later.
            for size in sizes:
                out_file.write(src_file + ',' + compilation_type + ',' + compilation_name + ',result,')
                out_file.write(str(compilation_id) + ',size,' + str(size) + '\n')

    # Return the total size
    return total_size


def main():
    parser = argparse.ArgumentParser(
        description='Perform code size measurement')

    # required arguments
    parser.add_argument('--exec-path', nargs=1, required=True,
        help='Path to the executable')
    parser.add_argument('--compilation-ids', nargs=1, required=True,
        help='File containing the compilation ids for the executable')
    parser.add_argument('--out', nargs=1, required=True,
        help='Output file to store executable results to')

    # optional arguments
    parser.add_argument('--exec-flags', nargs=1, required=False,
        help='Flags to use when executing the program')
    parser.add_argument('--debug', action='store_true', required=False,
        help='Enable debug when doing code size measurement')
    parser.set_defaults(exec_flags=[''],
                        debug=False)

    args = parser.parse_args(sys.argv[1:])
    exec_path       = os.path.abspath(args.exec_path[0])
    compilation_ids = os.path.abspath(args.compilation_ids[0])
    out             = os.path.abspath(args.out[0])

    exec_flags = args.exec_flags[0]
    debug = args.debug

    if not os.path.exists(exec_path):
        eprint ('-- Executable \'' + exec_path + '\' does not exist')
        return -1
    if not os.path.exists(compilation_ids):
        eprint ('-- Compilation id file \'' + compilation_ids + '\' does not exist')
        return -1

    # Measure the code size of the executable file and append the results to
    # the output file
    res = measure_executable(exec_path=exec_path,
                             compilation_ids=compilation_ids,
                             debug=debug,
                             out=out)
    if res < 0:
        eprint ('-- Failed to measure the size of \'' + exec_path + '\'')
        # Return a zero size through stdout, and return an error
        print (0)
        return -1
    else:
        # Return the size of the program through stdout
        print (res)
        return 0


if __name__ == '__main__':
    main()

