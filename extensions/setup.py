import shutil
import os
import platform
import sys
import subprocess
from pathlib import Path
from sysconfig import get_paths
from setuptools import setup, Extension, Command
from torch.utils import cpp_extension

IS_LINUX = 'Linux' in platform.platform()
EXT_USE_LMDB = os.environ.get("EXT_USE_LMDB")

class clean(Command):
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        p = Path('.')
        for child in p.iterdir():
            if 'build' == str(child): shutil.rmtree(child)
            if 'dist' == str(child): shutil.rmtree(child)
            if 'egg-info' in str(child): shutil.rmtree(child)
            if not child.is_dir(): continue
            for grandchild in child.iterdir():
                if '.so' == grandchild.suffix: grandchild.unlink()

class unittest(Command):
    description = 'run the unit tests'
    user_options = []

    def initialize_options(self):
        pass

    def finalize_options(self):
        pass

    def run(self):
        import subprocess
        import fnmatch
        import os
        tests = []
        tests_failed = []
        start_dir = '.'
        pattern = 'test_*.py'
        for dirpath, dirnames, filenames in os.walk(start_dir):
            for filename in fnmatch.filter(filenames, pattern):
                assert os.path.exists(dirpath+"/__init__.py"), "the directory " + dirpath + " without __init__.py contains the unit tests " + filename + "!"
                tests.append((dirpath, filename))
        for (d,t) in tests:
            test = os.path.join(d,t)
            try:
                print(test)
                rc = subprocess.call(f"cd {d} && python3 -B -m unittest -v {t}", shell=True)
                assert 0 == rc
            except:
                tests_failed.append(test)
        print("failed tests =", tests_failed)
        
def buildlib():
    ret = None
    for arg in sys.argv:
        if '--build-lib' in arg:
            ret = arg.split('=')[1]
    return ret

def gsl():
    path = os.getcwd() + '/GSL'
    if not os.path.isdir(path):
        proc = subprocess.run(['git', 'clone', 'https://github.com/microsoft/GSL.git'],
                              check=True)
    if os.path.isdir(path):
        proc = subprocess.run(['git', 'branch', '--show-current'],
                              stdout = subprocess.PIPE,
                              cwd = path,
                              check=True)
        current = proc.stdout.decode('ascii').strip()
        tag = 'v4.0.0'
        branch = tag.replace('v','b')
        if branch != current:
            proc = subprocess.run(['git', 'checkout', '-f', '-b', branch, tag],
                                  cwd = path,
                                  check=True)
    return ['-I' + path + '/include']

lmdb_conf_flags = ([f'-I{EXT_USE_LMDB}/include'],
                   [f'{EXT_USE_LMDB}/lib'],
                   [f'-L{EXT_USE_LMDB}/lib', '-llmdb'],
                   None)

gsl_conf_flags = gsl()

extra_cpp_flags = ['-g', '-O0', '-std=c++17', '-pedantic', '-Wall', '-Wextra', '-Wabi', '-ferror-limit=1', '-fvisibility=hidden', '-DPYDEF', '-DPYBIND11_DETAILED_ERROR_MESSAGES']
extra_cpp_flags += gsl_conf_flags
extra_ld_flags = []

setup(name='extensions',
      packages=[],
      ext_modules=[cpp_extension.CppExtension('bitarray.bitarray',
                                              sources=['bitarray/bitarray.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.mesh',
                                              sources=['mesh/mesh.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.tree',
                                              sources=['mesh/tree.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.multitree',
                                              sources=['mesh/multitree.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.property_tree',
                                              sources=['mesh/property_tree.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.star',
                                              sources=['mesh/star.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('mesh.children_traversal',
                                              sources=['mesh/children_traversal.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('iter.generated_iterator',
                                              sources=['iter/generated_iterator.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('iter.filtered_iterator',
                                              sources=['iter/filtered_iterator.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('iter.mapped_iterator',
                                              sources=['iter/mapped_iterator.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('iter.iter',
                                              sources=['iter/iter.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('graph.graph',
                                              sources=['graph/graph.cc',
                                                       'graph/feature.cc'],
                                              runtime_library_dirs=['$ORIGIN/../bitarray'] + lmdb_conf_flags[1],
                                              library_dirs= []
                                                          + ([f'{buildlib()}/bitarray'] if IS_LINUX else []),
                                              extra_compile_args=extra_cpp_flags + lmdb_conf_flags[0],
                                              extra_link_args= extra_ld_flags
                                                             + (['-l:bitarray.so'] if IS_LINUX else [])
                                                             + lmdb_conf_flags[2]),
                   cpp_extension.CppExtension('graphdb.graphdb',
                                              sources=['graphdb/graphdb.cc',
                                                       'graphdb/envpool.cc'],
                                              runtime_library_dirs = [] + lmdb_conf_flags[1],
                                              extra_compile_args=extra_cpp_flags + lmdb_conf_flags[0],
                                              extra_link_args= extra_ld_flags + lmdb_conf_flags[2]),
                   cpp_extension.CppExtension('threadpool.threadpool',
                                              sources=['threadpool/threadpool.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args= extra_ld_flags),
      ],
      cmdclass={'build_ext': cpp_extension.BuildExtension,
                'clean': clean,
                'test': unittest},
      install_requires=['torch'])
