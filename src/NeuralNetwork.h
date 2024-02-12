#pragma once


#include <torch/torch.h>


class NeuralNetwork : torch::nn::Module {



public:
	NeuralNetwork();
	std::vector<float> forward(const std::vector<float>& inputs);
	torch::Tensor forward(const torch::Tensor& input);
	void addTrainingData(std::vector<float> inputs, std::vector<float> outputs);
	void runTraining(int epochs);
	torch::jit::IValue NeuralNetwork::forward(torch::jit::IValue input);




private:

	int64_t n_inputs;

	int64_t n_outputs;
	std::vector<torch::Tensor> trainInputs;
	std::vector<torch::Tensor> trainOutputs;
	torch::nn::Linear linear1{ nullptr };
	torch::nn::Linear linear2{ nullptr };
	torch::nn::Sigmoid sig1{ nullptr };
	torch::nn::Softmax softmax{ nullptr };
	std::unique_ptr<torch::optim::SGD> optimiser;

	torch::jit::script::Module module;




};