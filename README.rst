Build
=====

python::

    python3 -m venv --copies env
    source ./env/bin/activate
    pip install --upgrade pip setuptools wheel
    pip freeze > /tmp/requirements.txt && pip uninstall -r /tmp/requirements.txt -y
    pip install -r ./requirements.txt
    pip install matplotlib expecttest hypothesis mypy pytest pandas networkx scipy lmdb

brew::

    brew install --with-toolchain llvm

build::
    git config user.name "Your Name Here"
    git config user.email your@email.example

    git submodule deinit --all
    git submodule sync
    git submodule update --init --recursive --recommend-shallow --jobs 11
    python -m sysconfig | grep symbolic

    # MACOSX_DEPLOYMENT_TARGET=14.8 CC=clang CXX=clang++ SETUPTOOLS_EXT_SUFFIX=.so  DEBUG=1 USE_DISTRIBUTED=0 USE_MKLDNN=0 USE_CUDA=0 USE_ROCM=0 BUILD_TEST=0 USE_FBGEMM=0 USE_NNPACK=0 USE_QNNPACK=0 USE_XNNPACK=0 USE_MPS=1
      EXT_USE_LLVM_CONFIG=/path/to/llvm\@14/14.0.6/bin/llvm-config  EXT_USE_LMDB=/path/to/lmdb/0.9.33/  python setup.py install

    #                               CC=clang CXX=clang++ SETUPTOOLS_EXT_SUFFIX=.so  DEBUG=1 USE_DISTRIBUTED=0 USE_MKLDNN=0 USE_CUDA=0 USE_ROCM=0 BUILD_TEST=0 USE_FBGEMM=0 USE_NNPACK=0 USE_QNNPACK=0 USE_XNNPACK=0 USE_MPS=0
      EXT_USE_LLVM_CONFIG=/usr/bin/llvm-config python setup.py  build --build-lib=./build/lib  install

docker/linux::
    export DOCKER_DEFAULT_PLATFORM=linux/amd64
    apt-get install apt-file locales emacs-nox wget
    emacs -nw /etc/locale.gen, locale-gen
    apt-get install python3 python3-venv python3-dev gcc g++ gdb git cmake valgrind
    apt-get install clang llvm liblmdb-dev libclang-14-dev libpolly-14-dev

unittest::

    python test/run_test.py  # all
    python3 -B test/test_license.py
    python -m unittest <extensions>/test_mps.py

lldb::

    lldb --source-before-file ../.lldbinit -O "breakpoint set -r 'LLT<.*>::compute'" -o "run" --  python -B ...

gdb::

    gdb -iex "set auto-load safe-path /" -iex "set breakpoint pending on" -ex "break permutation.h:32" -ex run --args python3 -m unittest -v solver_byz_test.Test.test_sum_of_time_signatures_of_syllables_constraint

priv::

    `<https://docs.github.com/en/repositories/creating-and-managing-repositories/duplicating-a-repository>`_

update::

    git checkout --track origin/viable/strict
    git fetch --tags upstream viable/strict
    git reset --hard upstream/viable/strict
    git branch e2.8.0 v2.8.0
    git branch e2.3.1_ e2.3.1
    git rebase --interactive e2.8.0 e2.3.1_
    git checkout e2.8.0
    git merge --ff-only e2.3.1_
    git branch --delete --force e2.3.1_

update README.md::

    git checkout main
    git fetch upstream main
    git reset --hard upstream/main
    git push -u origin main
    git checkout e2.3.1 -- README.md .github/FUNDING.yml

static analysis::

    mypy --strict --disable-error-code attr-defined --disable-error-code no-untyped-call --disable-error-code no-untyped-def -m <file as it appears in python's import statement>

includes::

    Properties > C/C++ General > Paths and Symbols > includes > GNU C/GNU C++
    ? find $(xcode-select -p) -name '*stdio*'
    /Library/Developer/CommandLineTools/SDKs/MacOSX12.sdk/usr/include
    /Library/Developer/CommandLineTools/SDKs/MacOSX12.sdk/usr/include/c++/v1

ramfs::

    ? hdiutil attach -nomount ram://$((2048*1024*20))  # 2048 memory blocks correspond to 1MB
    ? diskutil erasevolume APFS "RAMFS" </dev/disk5>
    ? hdiutil detach /Volumes/RAMFS

    or

    ? sudo mount_tmpfs ./ramfs

    ? RAMFS=/Volumes/RAMFS python -B -m unittest -v test_graphdb.Test.test_graph_iter_edge &

libstdc++-v3::

    mkdir gcc
    git init
    git remote add origin git://gcc.gnu.org/git/gcc.git
    git config core.sparseCheckout true
    git pull --depth=1 origin master

    # cat .git/info/sparse-checkout
    libstdc++-v3

    or

    git clone --depth=1 --single-branch <repo>

Codebase
========

Aten::

    /pytorch/aten/src/ATen/core/Tensor.h
    /pytorch/aten/src/ATen/test - APIs
    /pytorch/aten/src/ATen/Parallel.h

Eigen::

    Undefine NDEBUG in /pytorch/third_party/eigen/Eigen/src/Core/util/Macros.h,
    include it, and call eigen_assert().

MPS::

    /pytorch/aten/src/ATen/mps
    /pytorch/aten/src/ATen/native/mps

TVM::

    /pytorch/torch/_dynamo/backends/tvm.py

Web
===

:Aten: `<https://pytorch.org/cppdocs/api/namespace_at.html#namespace-at>`_
:c10: `<https://github.com/pytorch/pytorch/wiki/Software-Architecture-for-c10>`_
:c++: 
    `<https://pytorch.org/tutorials/intermediate/process_group_cpp_extension_tutorial.html>`_
    `<https://pytorch.org/cppdocs/>`_
:contrib: `<https://pytorch.org/docs/stable/community/contribution_guide.html>`_
:forums: `<https://discuss.pytorch.org>`_
:indexing: `<https://pytorch.org/cppdocs/notes/tensor_indexing.html>`_
:models: `<https://pytorch.org/vision/master/models.html>`_
:probability: `<https://pytorch.org/docs/stable/distributions.html>`_
:profile: `<https://pytorch.org/tutorials/recipes/recipes/profiler_recipe.html>`_
:wiki: `<https://github.com/pytorch/pytorch/wiki>`_

