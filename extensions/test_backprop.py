import unittest
import torch
import math

class Test(unittest.TestCase):
    
    # https://pytorch.org/tutorials/beginner/examples_tensor/polynomial_tensor.html
    def test_backprop(self):
        dtype = torch.float
        device = torch.device("cpu")
        # device = torch.device("cuda:0") # Uncomment this to run on GPU
        
        # Create random input and output data
        x = torch.linspace(-math.pi, math.pi, 2000, device=device, dtype=dtype)
        y = torch.sin(x)
        print("x = ", x)
        print("y = ", y)
        
        # Randomly initialize weights
        a = torch.randn((), device=device, dtype=dtype)
        b = torch.randn((), device=device, dtype=dtype)
        c = torch.randn((), device=device, dtype=dtype)
        d = torch.randn((), device=device, dtype=dtype)

        learning_rate = 1e-6
        for t in range(2000):
            # Forward pass: compute predicted y
            y_pred = a + b * x + c * x ** 2 + d * x ** 3
        
            # Compute and print loss
            loss = (y_pred - y).pow(2).sum().item()
            if t % 100 == 99:
                print(t, loss)
        
            # Backprop to compute gradients of a, b, c, d with respect to loss
            grad_y_pred = 2.0 * (y_pred - y)
            grad_a = grad_y_pred.sum()
            grad_b = (grad_y_pred * x).sum()
            grad_c = (grad_y_pred * x ** 2).sum()
            grad_d = (grad_y_pred * x ** 3).sum()
            if t % 100 == 99:
                print(grad_y_pred)
                print(grad_a)
                print(grad_b)
                print(grad_c)
                print(grad_d)
            
            # Update weights using gradient descent, 
            # w'' = w' - Error * X * Learning Rate
            # b'' = b' - Error     * Learning Rate
            a -= learning_rate * grad_a
            b -= learning_rate * grad_b
            c -= learning_rate * grad_c
            d -= learning_rate * grad_d

        print(f'Result: y = {a.item()} + {b.item()} x + {c.item()} x^2 + {d.item()} x^3')

    # https://pytorch.org/tutorials/beginner/examples_autograd/two_layer_net_autograd.html
    def test_backprop_autograd(self):
        dtype = torch.float
        device = torch.device("cpu")
        # device = torch.device("cuda:0")  # Uncomment this to run on GPU
        
        # Create Tensors to hold input and outputs.
        # By default, requires_grad=False, which indicates that we do not need to
        # compute gradients with respect to these Tensors during the backward pass.
        x = torch.linspace(-math.pi, math.pi, 2000, device=device, dtype=dtype)
        y = torch.sin(x)
        
        # Create random Tensors for weights. For a third order polynomial, we need
        # 4 weights: y = a + b x + c x^2 + d x^3
        # Setting requires_grad=True indicates that we want to compute gradients with
        # respect to these Tensors during the backward pass.
        a = torch.randn((), device=device, dtype=dtype, requires_grad=True)
        b = torch.randn((), device=device, dtype=dtype, requires_grad=True)
        c = torch.randn((), device=device, dtype=dtype, requires_grad=True)
        d = torch.randn((), device=device, dtype=dtype, requires_grad=True)
        
        learning_rate = 1e-6
        for t in range(2000):
            # Forward pass: compute predicted y using operations on Tensors.
            y_pred = a + b * x + c * x ** 2 + d * x ** 3
        
            # Compute and print loss using operations on Tensors.
            # Now loss is a Tensor of shape (1,)
            # loss.item() gets the scalar value held in the loss.
            loss = (y_pred - y).pow(2).sum()
            if t % 100 == 99:
                print(t, loss.item())
        
            # Use autograd to compute the backward pass. This call will compute the
            # gradient of loss with respect to all Tensors with requires_grad=True.
            # After this call a.grad, b.grad. c.grad and d.grad will be Tensors holding
            # the gradient of the loss with respect to a, b, c, d respectively.
            loss.backward()
        
            # Manually update weights using gradient descent. Wrap in torch.no_grad()
            # because weights have requires_grad=True, but we don't need to track this
            # in autograd.
            with torch.no_grad():
                a -= learning_rate * a.grad
                b -= learning_rate * b.grad
                c -= learning_rate * c.grad
                d -= learning_rate * d.grad
        
                # Manually zero the gradients after updating weights
                a.grad = None
                b.grad = None
                c.grad = None
                d.grad = None
        
        print(f'Result: y = {a.item()} + {b.item()} x + {c.item()} x^2 + {d.item()} x^3')
    
    # https://dair.ai/notebooks/machine%20learning/beginner/pytorch/neural%20network/2020/03/19/nn.html
    def test_backprop_nn(self):
        import torch
        import torch.nn as nn
        # In the data below, X represents the amount of hours studied and how
        # much time students spent sleeping, whereas y represent grades.
        X = torch.tensor(([2, 9], [1, 5], [3, 6]), dtype=torch.float) # 3 X 2 tensor
        y = torch.tensor(([92], [100], [89]), dtype=torch.float) # 3 X 1 tensor
        x_test = torch.tensor(([4, 8]), dtype=torch.float) # 1 X 2 tensor
        x_test_b = torch.tensor(([1, 9]), dtype=torch.float)

        # scale units to [0,1]
        X_max, _ = torch.max(X, 0)  # 0 == columns
        X = torch.div(X, X_max)
        
        x_test_max, _ = torch.max(x_test, 0)
        x_test = torch.div(x_test, x_test_max)
        
        x_test_b_max, _ = torch.max(x_test_b, 0)
        x_test_b = torch.div(x_test_b, x_test_b_max)

        y = y / 100  # max test score is 100

        class NeuralNetwork(nn.Module):
            def __init__(self):
                
                # https://pytorch.org/docs/stable/generated/torch.nn.Module.html
                # [...] an __init__() call to the parent class must be made
                # before assignment on the child.
                super(NeuralNetwork, self).__init__()

                # 3 layers
                self.inputSize = 2
                self.hiddenSize = 3
                self.outputSize = 1
                
                # weights
                self.W1 = nn.Parameter(torch.randn(self.inputSize, self.hiddenSize)) # 3 x 2 tensor
                self.W2 = nn.Parameter(torch.randn(self.hiddenSize, self.outputSize)) # 3 x 1 tensor
            
            # feedforward pass
            def forward(self, X):
                with torch.no_grad():
                    # On broadcasting https://pytorch.org/docs/stable/notes/broadcasting.html#broadcasting-semantics
                    #                 https://pytorch.org/docs/stable/generated/torch.matmul.html
                    self.z = torch.matmul(X, self.W1) # 3 x 3
                    self.z2 = self.sigmoid(self.z) # activation function
                    self.z3 = torch.matmul(self.z2, self.W2)
                    o = self.sigmoid(self.z3) # final activation function
                return o
                
            def sigmoid(self, s):
                return 1 / (1 + torch.exp(-s))
            
            def sigmoidPrime(self, s):
                # derivative of sigmoid
                return s * (1 - s)
            
            # backpropagation
            def backward(self, X, y, o):
                with torch.no_grad():
                    self.o_error = y - o # error in output
                    self.o_delta = self.o_error * self.sigmoidPrime(o) # derivative of sig to error
                    self.z2_error = torch.matmul(self.o_delta, torch.t(self.W2))
                    self.z2_delta = self.z2_error * self.sigmoidPrime(self.z2)
                    self.W1 += torch.matmul(torch.t(X), self.z2_delta)
                    self.W2 += torch.matmul(torch.t(self.z2), self.o_delta)
                
            def train(self, X, y):
                # forward + backward pass for training
                o = self.forward(X)
                self.backward(X, y, o)
                
            def saveWeights(self, model):
                # https://pytorch.org/tutorials/beginner/saving_loading_models.html
                print("state = ", model.state_dict())
                # torch.save(model.state_dict(), "NN")
                # torch.load("NN")
                
            def predict(self, x):
                print ("Predicted data based on trained weights: ")
                print ("x (scaled): \n" + str(x))
                print ("y_hat: \n" + str(self.forward(x)))
        
        NN = NeuralNetwork()
        for i in range(1000):  # trains the NN 1,000 times
            if (i % 100) == 0:
                print ("#" + str(i) + " Loss: " + str(torch.mean((y - NN(X))**2).detach().item()))  # mean sum squared loss
            NN.train(X, y)
        NN.saveWeights(NN)
        NN.predict(x_test)
        NN.predict(x_test_b)

    # https://pytorch.org/tutorials/beginner/blitz/neural_networks_tutorial.html
    def test_backprop_cnn(self):
        """
        Neural Networks
        ===============
        
        Neural networks can be constructed using the ``torch.nn`` package.
        
        Now that you had a glimpse of ``autograd``, ``nn`` depends on
        ``autograd`` to define models and differentiate them.
        An ``nn.Module`` contains layers, and a method ``forward(input)`` that
        returns the ``output``.
        
        For example, look at this network that classifies digit images:
        
        .. figure:: /_static/img/mnist.png
           :alt: convnet
        
           convnet
        
        It is a simple feed-forward network. It takes the input, feeds it
        through several layers one after the other, and then finally gives the
        output.
        
        A typical training procedure for a neural network is as follows:
        
        - Define the neural network that has some learnable parameters (or
          weights)
        - Iterate over a dataset of inputs
        - Process input through the network
        - Compute the loss (how far is the output from being correct)
        - Propagate gradients back into the network’s parameters
        - Update the weights of the network, typically using a simple update rule:
          ``weight = weight - learning_rate * gradient``
        
        Define the network
        ------------------
        
        Let’s define this network:
        """
        import torch
        import torch.nn as nn
        import torch.nn.functional as F

        class Net(nn.Module):
        
            def __init__(self):
                super(Net, self).__init__()
                # 1 input image channel, 6 output channels, 5x5 square convolution
                # kernel
                self.conv1 = nn.Conv2d(1, 6, 5)
                self.conv2 = nn.Conv2d(6, 16, 5)
                # an affine operation: y = Wx + b
                self.fc1 = nn.Linear(16 * 5 * 5, 120)  # 5*5 from image dimension 
                self.fc2 = nn.Linear(120, 84)
                self.fc3 = nn.Linear(84, 10)
        
            def forward(self, x):
                # Max pooling over a (2, 2) window
                x = F.max_pool2d(F.relu(self.conv1(x)), (2, 2))
                # If the size is a square, you can specify with a single number
                x = F.max_pool2d(F.relu(self.conv2(x)), 2)
                x = torch.flatten(x, 1) # flatten all dimensions except the batch dimension
                x = F.relu(self.fc1(x))
                x = F.relu(self.fc2(x))
                x = self.fc3(x)
                return x

        net = Net()
        print("network =", net)

        ########################################################################
        # You just have to define the ``forward`` function, and the ``backward``
        # function (where gradients are computed) is automatically defined for you
        # using ``autograd``.
        # You can use any of the Tensor operations in the ``forward`` function.
        #
        # The learnable parameters of a model are returned by ``net.parameters()``
        params = list(net.parameters())
        print("len =", len(params))
        print("conv1 =", params[0].size())  # conv1's .weight

        ########################################################################
        # Let's try a random 32x32 input.
        # Note: expected input size of this net (LeNet) is 32x32. To use this net on
        # the MNIST dataset, please resize the images from the dataset to 32x32.
        input = torch.randn(1, 1, 32, 32)
        out = net(input)  # calls net.forward
        print("net prediction =", out)

        ########################################################################
        # Zero the gradient buffers of all parameters and backprops with random
        # gradients:
        net.zero_grad()
        out.backward(torch.randn(1, 10))

        ########################################################################
        # .. note::
        #
        #     ``torch.nn`` only supports mini-batches. The entire ``torch.nn``
        #     package only supports inputs that are a mini-batch of samples, and not
        #     a single sample.
        #
        #     For example, ``nn.Conv2d`` will take in a 4D Tensor of
        #     ``nSamples x nChannels x Height x Width``.
        #
        #     If you have a single sample, just use ``input.unsqueeze(0)`` to add
        #     a fake batch dimension.
        #
        # Before proceeding further, let's recap all the classes you’ve seen so far.
        #
        # **Recap:**
        #   -  ``torch.Tensor`` - A *multi-dimensional array* with support for autograd
        #      operations like ``backward()``. Also *holds the gradient* w.r.t. the
        #      tensor.
        #   -  ``nn.Module`` - Neural network module. *Convenient way of
        #      encapsulating parameters*, with helpers for moving them to GPU,
        #      exporting, loading, etc.
        #   -  ``nn.Parameter`` - A kind of Tensor, that is *automatically
        #      registered as a parameter when assigned as an attribute to a*
        #      ``Module``.
        #   -  ``autograd.Function`` - Implements *forward and backward definitions
        #      of an autograd operation*. Every ``Tensor`` operation creates at
        #      least a single ``Function`` node that connects to functions that
        #      created a ``Tensor`` and *encodes its history*.
        #
        # **At this point, we covered:**
        #   -  Defining a neural network
        #   -  Processing inputs and calling backward
        #
        # **Still Left:**
        #   -  Computing the loss
        #   -  Updating the weights of the network
        #
        # Loss Function
        # -------------
        # A loss function takes the (output, target) pair of inputs, and computes a
        # value that estimates how far away the output is from the target.
        #
        # There are several different
        # `loss functions <https://pytorch.org/docs/nn.html#loss-functions>`_ under the
        # nn package .
        # A simple loss is: ``nn.MSELoss`` which computes the mean-squared error
        # between the output and the target.
        #
        # For example:
        output = net(input)
        target = torch.randn(10)  # a dummy target, for example
        target = target.view(1, -1)  # make it the same shape as output
        criterion = nn.MSELoss()
        
        loss = criterion(output, target)
        print(loss)

        ########################################################################
        # Now, if you follow ``loss`` in the backward direction, using its
        # ``.grad_fn`` attribute, you will see a graph of computations that looks
        # like this:
        #
        # ::
        #
        #     input -> conv2d -> relu -> maxpool2d -> conv2d -> relu -> maxpool2d
        #           -> flatten -> linear -> relu -> linear -> relu -> linear
        #           -> MSELoss
        #           -> loss
        #
        # So, when we call ``loss.backward()``, the whole graph is differentiated
        # w.r.t. the neural net parameters, and all Tensors in the graph that have
        # ``requires_grad=True`` will have their ``.grad`` Tensor accumulated with the
        # gradient.
        #
        # For illustration, let us follow a few steps backward:
        print(loss.grad_fn)  # MSELoss
        print(loss.grad_fn.next_functions[0][0])  # Linear
        print(loss.grad_fn.next_functions[0][0].next_functions[0][0])  # ReLU

        ########################################################################
        # Backprop
        # --------
        # To backpropagate the error all we have to do is to ``loss.backward()``.
        # You need to clear the existing gradients though, else gradients will be
        # accumulated to existing gradients.
        #
        #
        # Now we shall call ``loss.backward()``, and have a look at conv1's bias
        # gradients before and after the backward.
        net.zero_grad()     # zeroes the gradient buffers of all parameters
        
        print('conv1.bias.grad before backward')
        print(net.conv1.bias.grad)
        
        loss.backward()
        
        print('conv1.bias.grad after backward')
        print(net.conv1.bias.grad)

        ########################################################################
        # Now, we have seen how to use loss functions.
        #
        # **Read Later:**
        #
        #   The neural network package contains various modules and loss functions
        #   that form the building blocks of deep neural networks. A full list with
        #   documentation is `here <https://pytorch.org/docs/nn>`_.
        #
        # **The only thing left to learn is:**
        #
        #   - Updating the weights of the network
        #
        # Update the weights
        # ------------------
        # The simplest update rule used in practice is the Stochastic Gradient
        # Descent (SGD):
        #
        #      ``weight = weight - learning_rate * gradient``
        #
        # We can implement this using simple Python code:
        #
        # .. code:: python
        #
        #     learning_rate = 0.01
        #     for f in net.parameters():
        #         f.data.sub_(f.grad.data * learning_rate)
        #
        # However, as you use neural networks, you want to use various different
        # update rules such as SGD, Nesterov-SGD, Adam, RMSProp, etc.
        # To enable this, we built a small package: ``torch.optim`` that
        # implements all these methods. Using it is very simple:
        import torch.optim as optim
        
        # create your optimizer
        optimizer = optim.SGD(net.parameters(), lr=0.01)
        
        # in your training loop:
        optimizer.zero_grad()   # zero the gradient buffers
        output = net(input)
        loss = criterion(output, target)
        loss.backward()
        optimizer.step()    # Does the update

        ########################################################################
        # .. Note::
        #
        #       Observe how gradient buffers had to be manually set to zero using
        #       ``optimizer.zero_grad()``. This is because gradients are accumulated
        #       as explained in the `Backprop`_ section.

if '__main__' == __name__:
    unittest.main(verbosity=2)
