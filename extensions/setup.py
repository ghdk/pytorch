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
EXT_USE_LLVM_CONFIG = os.environ.get("EXT_USE_LLVM_CONFIG")
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

def llvm_config():
    import subprocess
    import re
    import sys
    cmd = [EXT_USE_LLVM_CONFIG, '--cppflags']
    proc = subprocess.run(cmd,
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE,
                          check=True)
    cppflags = proc.stdout.decode('ascii')
    cppflags = cppflags.strip()
    cppflags = cppflags.split(' ')
    cmd = [EXT_USE_LLVM_CONFIG, '--ldflags']
    proc = subprocess.run(cmd,
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE,
                          check=True)
    ldflags = proc.stdout.decode('ascii')
    ldflags = ldflags.strip()
    ldflags = ldflags.split(' ')
    cmd = [EXT_USE_LLVM_CONFIG, '--libs', 'all', '--ignore-libllvm']
    proc = subprocess.run(cmd,
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE,
                          check=True)
    libs = proc.stdout.decode('ascii')
    libs = libs.strip()
    libs = libs.split(' ')
    clanglibs = ['-lclangFrontend',
                 '-lclangSerialization',
                 '-lclangDriver',
                 '-lclangParse',
                 '-lclangSema',
                 '-lclangAnalysis',
                 '-lclangAST',
                 '-lclangASTMatchers',
                 '-lclangBasic',
                 '-lclangEdit',
                 '-lclangLex',
                 '-lclangTooling']
    libs = clanglibs + libs
    cmd = [EXT_USE_LLVM_CONFIG, '--libdir']
    proc = subprocess.run(cmd,
                          stdout = subprocess.PIPE,
                          stderr = subprocess.PIPE,
                          check=True)
    libdir = proc.stdout.decode('ascii')
    libdir = libdir.strip()
    libdir = [libdir]
    return (cppflags, libdir, ldflags, libs)

llvm_conf_flags = llvm_config() if EXT_USE_LLVM_CONFIG else None

lmdb_conf_flags = ([f'-I{EXT_USE_LMDB}/include'],
                   [f'{EXT_USE_LMDB}/lib'],
                   [f'-L{EXT_USE_LMDB}/lib', '-llmdb'],
                   None)

gsl_conf_flags = gsl()

sanitizer_flags = ['-fsanitize=undefined']

extra_cpp_flags  = ['-g', '-O0', '-std=c++17', '-pedantic', '-Wall', '-Wextra', '-Wabi', '-ferror-limit=1', '-fvisibility=hidden', '-DPYDEF', '-DPYBIND11_DETAILED_ERROR_MESSAGES']
extra_cpp_flags += gsl_conf_flags
extra_cpp_flags += sanitizer_flags
extra_ld_flags  = []
extra_ld_flags += sanitizer_flags

spl = lambda p: ["-I"+path+p for path in sys.path if 'site-packages' in path]
with open("./llvmast/cached_cflags.py", 'w') as f:
    print(f"""CFLAGS={extra_cpp_flags + llvm_conf_flags[0] + lmdb_conf_flags[0] +
                      ["-I" + get_paths()['include']] +
                      ["-resource-dir", "/Library/Developer/CommandLineTools/usr/lib/clang/15.0.0"] +
                      ["-isysroot", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"] +
                      ["-I/usr/local/include"] +
                      spl("/torch/include") +
                      spl("/torch/include/torch/csrc/api/include") +
                      spl("/torch/include/TH") + 
                      spl("/torch/include/THC") +
                      ["-Wno-unused-variable", "-Wno-unused-parameter"]}""",
          file=f)

setup(name='extensions',
      packages=[],
      ext_modules=[
          cpp_extension.CppExtension('bitarray.bitarray',
                                     sources=['bitarray/bitarray.cc'],
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
                                     sources=[
                                         'graph/graph.cc',
                                         'graph/feature.cc'
                                     ],
                                     runtime_library_dirs=['$ORIGIN/../bitarray'] + lmdb_conf_flags[1],
                                     library_dirs= []
                                                 + ([f'{buildlib()}/bitarray'] if IS_LINUX else []),
                                     extra_compile_args=extra_cpp_flags + lmdb_conf_flags[0],
                                     extra_link_args= extra_ld_flags
                                                    + (['-l:bitarray.so'] if IS_LINUX else [])
                                                    + lmdb_conf_flags[2]),
          cpp_extension.CppExtension('graphdb.graphdb',
                                     sources=[
                                         'graphdb/graphdb.cc',
                                         'graphdb/envpool.cc'
                                     ],
                                     runtime_library_dirs = [] + lmdb_conf_flags[1],
                                     extra_compile_args=extra_cpp_flags + lmdb_conf_flags[0],
                                     extra_link_args= extra_ld_flags + lmdb_conf_flags[2]),
      ] + ([
          cpp_extension.CppExtension('llvmast.llvmast',
                                     sources=['llvmast/llvmast.cc'],
                                     runtime_library_dirs = ['$ORIGIN/../graph']
                                                          + ['$ORIGIN/../graphdb']
                                                          + llvm_conf_flags[1] + lmdb_conf_flags[1],
                                     library_dirs = []
                                                  + ([f'{buildlib()}/graph'] if IS_LINUX else [])
                                                  + ([f'{buildlib()}/graphdb'] if IS_LINUX else []),
                                     extra_compile_args = extra_cpp_flags + llvm_conf_flags[0] + lmdb_conf_flags[0],
                                     extra_link_args = extra_ld_flags
                                                     + llvm_conf_flags[2] + llvm_conf_flags[3]
                                                     + (['-ltinfo'] if IS_LINUX else [])
                                                     + (['-l:graph.so'] if IS_LINUX else [])
                                                     + (['-l:graphdb.so'] if IS_LINUX else [])
                                                     + lmdb_conf_flags[2]),
      ] if EXT_USE_LLVM_CONFIG else []) +
      [
          cpp_extension.CppExtension('theory',
                                     sources=['theory/module.cc',
                                              'theory/linalg/matrix.cc',
                                              'theory/parallel/parallel.cc'],
                                     extra_compile_args=extra_cpp_flags,
                                     extra_link_args= extra_ld_flags),
      ],
      cmdclass={'build_ext': cpp_extension.BuildExtension,
                'clean': clean,
                'test': unittest},
      install_requires=['torch'])
