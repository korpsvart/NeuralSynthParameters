

#include <torch/torch.h>
#include <torch/script.h>
#include "NeuralNetwork.h"

NeuralNetwork::NeuralNetwork()
{

	//Use Torchscript to load neural network
	//trained with python ddx7 library


	try {
		// Deserialize the ScriptModule from a file using torch::jit::load().
		module = torch::jit::load("C:/Users/Ricky/projects/music/fm_synth_libtorch/traced_ssssm/traced_model.pt");


	}
	catch (const c10::Error& e) {
		std::cerr << "error loading the model\n";
	}


	std::cout << "Loaded model\n";


	//Printing some infos about the loaded model
	int i = 0;
	for (auto m : module.modules())
	{
		//std::cout << printf(m) << std::endl;
	}

}

std::vector<float> NeuralNetwork::forward(const std::vector<float>& inputs)
{
	torch::Tensor in_t = torch::empty({ 1, n_inputs });
	for (long i = 0; i < n_inputs; i++)
	{
		in_t[0][i] = inputs[i];
	}
	torch::Tensor out_t = forward(in_t);
	std::vector<float> outputs(n_outputs);
	for (long i = 0; i < n_outputs; i++)
	{
		outputs[i] = out_t[0][i].item<float>();
	}
	return outputs;
}

void NeuralNetwork::addTrainingData(std::vector<float> inputs, std::vector<float> outputs)
{


	std::cout << "input: " << inputs << std::endl;
	std::cout << "output: " << outputs << std::endl;
	assert(inputs.size() == (unsigned long)n_inputs);
	assert(outputs.size() == (unsigned long)n_outputs);
	torch::Tensor in_t = torch::from_blob((float*)(inputs.data()), inputs.size()).clone();

	torch::Tensor out_t = torch::from_blob((float*)(outputs.data()), outputs.size()).clone();

	trainInputs.push_back(in_t);
	trainOutputs.push_back(out_t);

}

void NeuralNetwork::runTraining(int epochs)
{
	torch::Tensor inputs = torch::cat(trainInputs).reshape(
		{ (signed long)trainInputs.size(), trainInputs[0].sizes()[0] }
	);

	torch::Tensor outputs = torch::cat(trainOutputs).reshape(
		{ (signed long)trainOutputs.size(), trainOutputs[0].sizes()[0] }
	);

	float loss{ 0 }, pLoss{ 1000 }, dLoss{ 1000 };
	for (int i = 0; i < epochs; ++i) {
		// clear out the optimiser
		this->optimiser->zero_grad();
		auto loss_result = torch::mse_loss(forward(inputs), outputs);
		loss = loss_result.item<float>();
		loss_result.backward();
		this->optimiser->step();
		// now compute the change in loss
		// and exit if it is too low
		dLoss = pLoss - loss;
		pLoss = loss; // prev loss
		if (i > 0) {// only after first iteration
			if (dLoss < 0.000001) {
				std::cout << "dLoss very low, exiting at iter " << i << std::endl;
				break;
			}
		}
	}

}

torch::Tensor NeuralNetwork::forward(const torch::Tensor& input)
{

	std::vector<torch::jit::IValue> my_input;
	my_input.push_back(input);

	//Test input
	//my_input.push_back(torch::ones({ 16, 2, 1000 }));

	torch::jit::IValue out = module.forward(my_input);
	return out.toTensor();
}

torch::jit::IValue NeuralNetwork::forward(torch::jit::IValue input)
{

	std::vector<torch::jit::IValue> my_input;
	my_input.push_back(input);

	return module.forward(my_input);
}