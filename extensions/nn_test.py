import unittest
import torch
import math
# https://pytorch.org/docs/stable/tensorboard.html
from torch.utils.tensorboard import SummaryWriter

# from numpy import asarray
# from keras.models import Sequential
# from keras.layers import Conv1D
# from keras.layers import Conv2D
        
class Test(unittest.TestCase):

    _image = '../TF~assets/8628754308_9d712354d4_o.jpg'

    def test_nn_1d(self):
        # http://karpathy.github.io/2019/04/25/recipe/
        # https://towardsdatascience.com/debugging-neural-networks-with-pytorch-and-w-b-2e20667baf72
        
        # https://discuss.pytorch.org/t/check-gradient-flow-in-network/15063/7
        # https://cs231n.github.io/neural-networks-3/#gradcheck
        # https://cs231n.github.io/optimization-1/#gradcompute
        # https://www.youtube.com/watch?v=P6EtCVrvYPU
        # pip install flashtorch
        #             torchvision
        
        # wandb
        # https://github.com/ayulockin/debugNNwithWandB  -  code examples
        # https://wandb.ai/ayush-thakur/debug-neural-nets/reports/Visualizing-and-Debugging-Neural-Networks-with-PyTorch-and-W-B--Vmlldzo2OTUzNA
        # dropout - batch normalisation - https://arxiv.org/abs/1801.05134
    
        # unit test
        # https://thenerdstation.medium.com/how-to-unit-test-machine-learning-code-57cf6fd81765
        # https://github.com/suriyadeepan/torchtest
        
        # inspect
        # https://github.com/jettify/pytorch-inspect
        
        # A subclass to use as reference
        class NeuralNetwork(torch.nn.Module):
            def __init__(self, layers, tracing):
                super(NeuralNetwork, self).__init__()
                self._layers = layers
                self._tracing = tracing
            def forward(self, x):
                if self._tracing: print(self._layers)
                for layer in self._layers:
                    if self._tracing: print("in = ", x)
                    x = layer(x)
                    if self._tracing: print("out = ", x)
                return x
            def __getitem__(self, item):
                return self._layers[item]
            def _forward_hook(self, module, input, output):
                print("input = ", input)
                print("output = ", output)
        
        writer_scope = 'runs/test_nn_1d'
        writer = SummaryWriter(writer_scope)

        # Create Tensors to hold input and outputs.
        x = torch.linspace(-math.pi, math.pi, 2000)
        y = torch.sin(x)
        
        # For this example, the output y is a linear function of (x, x^2, x^3), so
        # we can consider it as a linear layer neural network. Let's prepare the
        # tensor (x, x^2, x^3).
        p = torch.tensor([1, 2, 3])
        xx = x.unsqueeze(-1).pow(p)
        print(xx)

        # In the above code, x.unsqueeze(-1) has shape (2000, 1), and p has shape
        # (3,), for this case, broadcasting semantics will apply to obtain a tensor
        # of shape (2000, 3)
        
        # Use the nn package to define our model as a sequence of layers. nn.Sequential
        # is a Module which contains other Modules, and applies them in sequence to
        # produce its output. The Linear Module computes output from input using a
        # linear function, and holds internal Tensors for its weight and bias.
        # The Flatten layer flatens the output of the linear layer to a 1D tensor,
        # to match the shape of `y`.
        layers = torch.nn.Sequential(
            torch.nn.Linear(3, 1),
            torch.nn.Flatten(0, 1)
        )
        model = NeuralNetwork(layers, True)
        
        # The nn package also contains definitions of popular loss functions; in this
        # case we will use Mean Squared Error (MSE) as our loss function.
        loss_fn = torch.nn.MSELoss(reduction='sum')
        
        learning_rate = 1e-6
        for t in range(2000):
        
            # Forward pass: compute predicted y by passing x to the model. Module objects
            # override the __call__ operator so you can call them like functions. When
            # doing so you pass a Tensor of input data to the Module and it produces
            # a Tensor of output data.
            y_pred = model(xx)
        
            # Compute and print loss. We pass Tensors containing the predicted and true
            # values of y, and the loss function returns a Tensor containing the
            # loss.
            loss = loss_fn(y_pred, y)

            if t % 100 == 99:
                writer.add_scalar(writer_scope+'/loss', loss.item(), t,)
        
            # Zero the gradients before running the backward pass.
            model.zero_grad()
        
            # Backward pass: compute gradient of the loss with respect to all the learnable
            # parameters of the model. Internally, the parameters of each Module are stored
            # in Tensors with requires_grad=True, so this call will compute gradients for
            # all learnable parameters in the model.
            loss.backward()
        
            # Update the weights using gradient descent. Each parameter is a Tensor, so
            # we can access its gradients like we did before.
            with torch.no_grad():
                for param in model.parameters():
                    param -= learning_rate * param.grad
        
        # You can access the first layer of `model` like accessing the first item of a list
        linear_layer = model[0]
        
        # For linear layer, its parameters are stored as `weight` and `bias`.
        print(f'Result: y = {linear_layer.bias.item()} + {linear_layer.weight[:, 0].item()} x + {linear_layer.weight[:, 1].item()} x^2 + {linear_layer.weight[:, 2].item()} x^3')

        writer.add_graph(model,xx)
        writer.close()


    
    # python3 -B ../tensorflow/python/examples/test_neural_net_convolutional.py Test.test_cnn_1d
    # def test_cnn_1d(self):
    #     # define input data
    #     data = asarray([0, 0, 0, 1, 1, 0, 0, 0])
    #     # The input to Keras must be three dimensional for a 1D convolutional
    #     # layer.
    #     # The first dimension refers to each input sample; in this case, we only
    #     # have one sample. The second dimension refers to the length of each
    #     # sample; in this case, the length is eight. The third dimension refers
    #     # to the number of channels in each sample; in this case, we only have a
    #     # single channel.
    #     # Therefore, the shape of the input array will be [1, 8, 1].
    #     data = data.reshape(1, 8, 1)
    #     # create model
    #     model = Sequential()
    #     model.add(Conv1D(1, 3, input_shape=(8, 1)))
    #     # define a vertical line detector
    #     weights = [asarray([[[0]],[[1]],[[0]]]), asarray([0.0])]
    #     # store the weights in the model
    #     model.set_weights(weights)
    #     # apply filter to input data
    #     yhat = model.predict(data)
    #     print("yhat=")
    #     print(yhat)

    # # python3 -B ../tensorflow/python/examples/test_neural_net_convolutional.py Test.test_cnn_2d
    # def test_cnn_2d(self):
    #     # define input data
    #     data = [[0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0],
    #             [0, 0, 0, 1, 1, 0, 0, 0]]
    #     data = asarray(data)
    #     # The input to a Conv2D layer must be four-dimensional.
    #     # The first dimension defines the samples; in this case, there is only a
    #     # single sample. The second dimension defines the number of rows; in
    #     # this case, eight. The third dimension defines the number of columns,
    #     # again eight in this case, and finally the number of channels, which is
    #     # one in this case.
    #     # Therefore, the input must have the four-dimensional shape [samples,
    #     # rows, columns, channels] or [1, 8, 8, 1] in this case.
    #     data = data.reshape(1, 8, 8, 1)
    #     # create model
    #     model = Sequential()
    #     model.add(Conv2D(1, (3,3), input_shape=(8, 8, 1)))
    #     # define a vertical line detector
    #     detector = [[[[0]],[[1]],[[0]]],
    #                 [[[0]],[[1]],[[0]]],
    #                 [[[0]],[[1]],[[0]]]]
    #     weights = [asarray(detector), asarray([0.0])]
    #     # store the weights in the model
    #     model.set_weights(weights)
    #     # confirm they were stored
    #     print(model.get_weights())
    #     # apply filter to input data
    #     yhat = model.predict(data)
    #     for r in range(yhat.shape[1]):
    #         # print each column in the row
    #         print([yhat[0,r,c,0] for c in range(yhat.shape[2])])

    # def test_cnn_fashion(self):
    #     # https://www.datacamp.com/community/tutorials/convolutional-neural-networks-python
    #     # https://www.edureka.co/blog/convolutional-neural-network/
    #     # https://www.tensorflow.org/tutorials/images/cnn
    #     # from scratch
    #     # https://gist.github.com/JiaxiangZheng/a60cc8fe1bf6e20c1a41abc98131d518
    #     # https://q-viper.github.io/2020/06/05/convolutional-neural-networks-from-scratch-on-python/
        
    #     # Multiblock Adaptive Convolution Kernel Neural Network for Fault Diagnosis in a Large-Scale Industrial Process
    #     # visualise feature maps
    #     # https://www.analyticsvidhya.com/blog/2020/11/tutorial-how-to-visualize-feature-maps-directly-from-cnn-layers/
    #     pass

    # # python3 -B ../tensorflow/python/examples/test_neural_net_convolutional.py Test.test_cnn_visualise_filters
    # def test_cnn_visualise_filters(self):
    #     # source https://machinelearningmastery.com/how-to-visualize-filters-and-feature-maps-in-convolutional-neural-networks/
    #     from keras.applications.vgg16 import VGG16  # https://keras.io/api/applications/
    #                                                 # https://arxiv.org/abs/1409.1556
    #     from matplotlib import pyplot
    #     # load the model - https://www.tensorflow.org/guide/keras/save_and_serialize
    #     model = VGG16()
    #     model.summary()
    #     # summarize filter shapes
    #     for layer in model.layers:
    #         # check for convolutional layer
    #         if 'conv' not in layer.name:
    #             continue
    #         # get filter weights
    #         filters, biases = layer.get_weights()
    #         print(layer.name, filters.shape)
    #     # retrieve weights from the second hidden layer
    #     filters, biases = model.layers[1].get_weights()
    #     # normalize filter values to 0-1 so we can visualize them
    #     f_min, f_max = filters.min(), filters.max()
    #     filters = (filters - f_min) / (f_max - f_min)
    #     # plot first few filters
    #     n_filters, ix = 6, 1
    #     for i in range(n_filters):
    #         # get the filter
    #         f = filters[:, :, :, i]
    #         # plot each channel separately
    #         for j in range(3):
    #             # specify subplot and turn of axis
    #             ax = pyplot.subplot(n_filters, 3, ix)
    #             ax.set_xticks([])
    #             ax.set_yticks([])
    #             # plot filter channel in grayscale
    #             pyplot.imshow(f[:, :, j], cmap='gray')
    #             ix += 1
    #     # show the figure
    #     pyplot.show()

    # # python3 -B ../tensorflow/python/examples/test_neural_net_convolutional.py Test.test_cnn_visualise_feature_maps_first_hidden
    # def test_cnn_visualise_feature_maps_first_hidden(self):
    #     # source https://machinelearningmastery.com/how-to-visualize-filters-and-feature-maps-in-convolutional-neural-networks/
    #     from keras.applications.vgg16 import VGG16
    #     from keras.applications.vgg16 import preprocess_input
    #     from keras.preprocessing.image import load_img
    #     from keras.preprocessing.image import img_to_array
    #     from keras.models import Model
    #     from matplotlib import pyplot
    #     from numpy import expand_dims
    #     # load the model
    #     model = VGG16()
    #     model.summary()
    #     # summarize feature map shapes
    #     for i in range(len(model.layers)):
    #         layer = model.layers[i]
    #         # check for convolutional layer
    #         if 'conv' not in layer.name:
    #             continue
    #         # summarize output shape
    #         print(i, layer.name, layer.output.shape)
    #     # redefine model to output right after the first hidden layer
    #     model = Model(inputs=model.inputs, outputs=model.layers[1].output)
    #     model.summary()
    #     # load the image with the required shape
    #     img = load_img(__class__._image, target_size=(224, 224))
    #     # convert the image to an array
    #     img = img_to_array(img)
    #     # expand dimensions so that it represents a single 'sample'
    #     img = expand_dims(img, axis=0)
    #     # prepare the image (e.g. scale pixel values for the vgg)
    #     img = preprocess_input(img)
    #     # get feature map for first hidden layer
    #     feature_maps = model.predict(img)
    #     # plot all 64 maps in an 8x8 squares
    #     square = 8
    #     ix = 1
    #     for _ in range(square):
    #         for _ in range(square):
    #             # specify subplot and turn of axis
    #             ax = pyplot.subplot(square, square, ix)
    #             ax.set_xticks([])
    #             ax.set_yticks([])
    #             # plot filter channel in grayscale
    #             pyplot.imshow(feature_maps[0, :, :, ix-1], cmap='gray')
    #             ix += 1
    #     # show the figure
    #     pyplot.show()

    # # python3 -B ../tensorflow/python/examples/test_neural_net_convolutional.py Test.test_cnn_visualise_feature_maps_model_blocks
    # def test_cnn_visualise_feature_maps_model_blocks(self):
    #     # source https://machinelearningmastery.com/how-to-visualize-filters-and-feature-maps-in-convolutional-neural-networks/
    #     from keras.applications.vgg16 import VGG16
    #     from keras.applications.vgg16 import preprocess_input
    #     from keras.preprocessing.image import load_img
    #     from keras.preprocessing.image import img_to_array
    #     from keras.models import Model
    #     from matplotlib import pyplot
    #     from numpy import expand_dims
    #     # load the model
    #     model = VGG16()
    #     # redefine model to output right after the first hidden layer
    #     ixs = [2, 5, 9, 13, 17]
    #     outputs = [model.layers[i].output for i in ixs]
    #     model = Model(inputs=model.inputs, outputs=outputs)
    #     # load the image with the required shape
    #     img = load_img(__class__._image, target_size=(224, 224))
    #     # convert the image to an array
    #     img = img_to_array(img)
    #     # expand dimensions so that it represents a single 'sample'
    #     img = expand_dims(img, axis=0)
    #     # prepare the image (e.g. scale pixel values for the vgg)
    #     img = preprocess_input(img)
    #     # get feature map for first hidden layer
    #     feature_maps = model.predict(img)
    #     # plot the output from each block
    #     square = 8
    #     for fmap in feature_maps:
    #         # plot all 64 maps in an 8x8 squares
    #         ix = 1
    #         for _ in range(square):
    #             for _ in range(square):
    #                 # specify subplot and turn of axis
    #                 ax = pyplot.subplot(square, square, ix)
    #                 ax.set_xticks([])
    #                 ax.set_yticks([])
    #                 # plot filter channel in grayscale
    #                 pyplot.imshow(fmap[0, :, :, ix-1], cmap='gray')
    #                 ix += 1
    #         # show the figure
    #         pyplot.show()

if '__main__' == __name__:
    unittest.main(verbosity=2)
