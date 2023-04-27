Build
=====

python::

    python3 -m venv env
    source ./env/bin/activate
    pip install --upgrade pip setuptools wheel mypy
    pip freeze > /tmp/requirements.txt && pip uninstall -r /tmp/requirements.txt -y
    pip install -r ./requirements.txt
    pip install matplotlib expecttest hypothesis mypy pytest

build::

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

lldb::

    lldb --source-before-file ../.lldbinit -O "breakpoint set -r 'LLT<.*>::compute'" -o "run" --  python -B ...

gdb::

    gdb -iex "set auto-load safe-path /" -iex "set breakpoint pending on" -ex "break permutation.h:32" -ex run --args python3 constraints/solver_test.py Test.test_constraints_solver_permutation

update::

    git checkout --track origin/viable/strict
    git fetch --tags upstream viable/strict
    git merge upstream/viable/strict
    git checkout v1.13.0 -b e1.13.0
    git rev-list --reverse --no-merges <ancestor of old branch>..<head of old branch> | git cherry-pick --stdin -X ours
    git diff <old branch>:./extensions <new branch>:./extensions

..  when asked for empty commits do -skip

static analysis::

    mypy --disallow-untyped-defs <file>

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

TODOs
=====

====  contrib  =================================================================

`<https://pytorch.org/docs/stable/community/contribution_guide.html>`_

====  Federated learning  ======================================================

`<https://ai.googleblog.com/2017/04/federated-learning-collaborative.html>`_

====  CNNs  ====================================================================
====  RNNs  ====================================================================

recurrent neural network and Legendre polynomials

====  Model Prunning  ==========================================================

`<https://pytorch.org/tutorials/beginner/basics/optimization_tutorial.html>`_

====  Probability  =============================================================

`<https://pytorch.org/docs/stable/distributions.html>`_

====  models  ==================================================================

`<https://pytorch.org/vision/master/models.html>`_

====  optimiser  ===============================================================

====  transfer learning  =======================================================

====  GPU/CUDA==================================================================

GPU operators for bitmap, and GPU execution.

====  pipeline  ================================================================

https://theaisummer.com/tfx/

====  Algorithms  ==============================================================

- Ensemble Learning Algorithms (Random Forests, XGBoost, LightGBM, CatBoost)
- Explanatory Algorithms (Linear Regression, Logistic Regression, SHAP, LIME)
- Clustering Algorithms (k-Means, Hierarchical Clustering)
- Dimensionality Reduction Algorithms (PCA, LDA)
- Similarity Algorithms (KNN, Euclidean Distance, Cosine, Levenshtein,
                         Jaro-Winkler, SVD)

====  large language models (LLMs)  ============================================
- BLOOM (BigScience Language Open-science Open-access Multilingual)
- Gopher, Chinchilla, and PaLM
- GPT-3

====  memory  ==================================================================

https://www.sicara.fr/blog-technique/2019-28-10-deep-learning-memory-usage-and-pytorch-optimization-tricks
                         
================================================================================
Object detection/segmentation models, Transfer Learning, CNN, Autoencoders,
GANs, Transformers, LSTMs, Ensemble Learning, Continual Learning, AutoML,
Meta Learning, Reinforcement Learning, Domain Adaptation, Zero-shot Learning,
OCR.

Web
===

forums::
    `<https://discuss.pytorch.org>`_
c++::
    `<https://pytorch.org/tutorials/intermediate/process_group_cpp_extension_tutorial.html>`_
    `<https://pytorch.org/cppdocs/>`_
c10::
    `<https://github.com/pytorch/pytorch/wiki/Software-Architecture-for-c10>`_
Aten::
    `<https://pytorch.org/cppdocs/api/namespace_at.html#namespace-at>`_
