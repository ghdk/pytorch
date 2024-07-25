Build
=====

python::

    python3 -m venv env
    source ./env/bin/activate
    pip install --upgrade pip setuptools wheel mypy
    pip freeze > /tmp/requirements.txt && pip uninstall -r /tmp/requirements.txt -y
    pip install -r ./requirements.txt
    pip install matplotlib expecttest hypothesis mypy pytest pandas networkx scipy

build::

    git submodule deinit --all
    git submodule sync
    git submodule update --init --recursive --jobs 11
    MACOSX_DEPLOYMENT_TARGET=13.2 CC=clang CXX=clang++  DEBUG=1 USE_DISTRIBUTED=0 USE_MKLDNN=0 USE_CUDA=0 USE_ROCM=0 BUILD_TEST=0 USE_FBGEMM=0 USE_NNPACK=0 USE_QNNPACK=0 USE_XNNPACK=0 USE_MPS=0  python setup.py install
                                                                                                                                                                                                         build -j11 install test clean

docker/linux::
    apt-get install apt-file aptitude
    aptitude install locales emacs-nox wget
    emacs -nw /etc/locale.gen, locale-gen
    aptitude install python3 python3-venv python3-dev gcc g++ gdb git cmake valgrind

unittest::

    python test/run_test.py  # all
    python3 -B test/test_license.py
    python -m unittest <extensions>/test_mps.py

lldb::

    lldb --source-before-file ../.lldbinit -O "breakpoint set -r 'LLT<.*>::compute'" -o "run" --  python -B ...

gdb::

    gdb -iex "set auto-load safe-path /" -iex "set breakpoint pending on" -ex "break permutation.h:32" -ex run --args python3 -m unittest -v solver_byz_test.Test.test_sum_of_time_signatures_of_syllables_constraint

update::

    git checkout --track origin/viable/strict
    git fetch --tags upstream viable/strict
    git merge upstream/viable/strict
    git checkout v1.13.0 -b e1.13.0
    git rev-list --reverse --no-merges <ancestor of old branch>..<head of old branch> | git cherry-pick --stdin -X ours
    git diff <old branch>:./extensions <new branch>:./extensions

..  when asked for empty commits do -skip

static analysis::

    mypy --strict --disable-error-code attr-defined --disable-error-code no-untyped-call --disable-error-code no-untyped-def -m <file as it appears in python's import statement>

includes::

    Properties > C/C++ General > Paths and Symbols > includes > GNU C/GNU C++
    ? find $(xcode-select -p) -name '*stdio*'
    /Library/Developer/CommandLineTools/SDKs/MacOSX12.sdk/usr/include
    /Library/Developer/CommandLineTools/SDKs/MacOSX12.sdk/usr/include/c++/v1

Codebase
========

Eigen::

    Undefine NDEBUG in /pytorch/third_party/eigen/Eigen/src/Core/util/Macros.h,
    include it, and call eigen_assert().

Aten::

    /pytorch/aten/src/ATen/core/Tensor.h
    /pytorch/aten/src/ATen/test - APIs

MPS::

    /pytorch/aten/src/ATen/mps
    /pytorch/aten/src/ATen/native/mps

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

