Build
=====

python::

    python3 -m venv env
    source ./env/bin/activate
    pip install --upgrade pip setuptools wheel
    pip freeze > /tmp/requirements.txt && pip uninstall -r /tmp/requirements.txt -y
    pip install -r ./requirements.txt
    pip install matplotlib

build::

    git submodule sync
    git submodule update --init --recursive --jobs 0
    MACOSX_DEPLOYMENT_TARGET=12.4 CC=clang CXX=clang++ DEBUG=1 USE_CUDA=0 USE_ROCM=0  python setup.py install

unittest::

    python3 -B test/test_license.py

lldb::

    lldb --source-before-file ../.lldbinit -O "breakpoint set -r 'LLT<.*>::compute'" -o "run" --  python -B ...

NDEBUG
======

Eigen::

   Undefine NDEBUG in /eigen/Eigen/src/Core/util/Macros.h, include it, and
   call eigen_assert().

TODOs
=====

====  contrib  =================================================================

`<https://pytorch.org/docs/stable/community/contribution_guide.html>`_

====  Federated learning  ======================================================

`<https://ai.googleblog.com/2017/04/federated-learning-collaborative.html>`_

====  CNNs  ====================================================================

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

================================================================================
Object detection/segmentation models, Transfer Learning, CNN, Autoencoders,
GANs, Transformers, LSTMs, Ensemble Learning, Continual Learning, AutoML,
Meta Learning, Reinforcement Learning, Domain Adaptation, Zero-shot Learning,
OCR.
