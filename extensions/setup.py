import shutil
import platform
import sys
from pathlib import Path
from setuptools import setup, Extension, Command
from torch.utils import cpp_extension

IS_LINUX = 'Linux' in platform.platform()

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
        pattern = '*_test.py'
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

extra_cpp_flags = ['-g', '-O0', '-std=c++17', '-pedantic', '-Wall', '-Wextra', '-Wabi', '-DPYDEF', '-DPYBIND11_DETAILED_ERROR_MESSAGES']
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
                   cpp_extension.CppExtension('graph.graph',
                                              sources=['graph/graph.cc'],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags),
                   cpp_extension.CppExtension('graph.converter',
                                              sources=['graph/converter.cc'],
                                              runtime_library_dirs=['$ORIGIN/../bitarray'],
                                              library_dirs=[] + [f'{buildlib()}/bitarray/'] if IS_LINUX else [],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags + ['-l:bitarray.so'] if IS_LINUX else []),
                   cpp_extension.CppExtension('graph.accessor',
                                              sources=['graph/accessor.cc'],
                                              runtime_library_dirs=['$ORIGIN/../bitarray'],
                                              library_dirs=[] + [f'{buildlib()}/bitarray/'] if IS_LINUX else [],
                                              extra_compile_args=extra_cpp_flags,
                                              extra_link_args=extra_ld_flags + ['-l:bitarray.so'] if IS_LINUX else []),
      ],
      cmdclass={'build_ext': cpp_extension.BuildExtension,
                'clean': clean,
                'test': unittest},
      install_requires=['torch'])
