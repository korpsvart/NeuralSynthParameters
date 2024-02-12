

#include <torch/torch.h>
#include <torch/script.h>
#include "NeuralNetwork.h"

NeuralNetwork::NeuralNetwork()
{

	//Use Torchscript to load neural network
	//trained with python SSSSM-DDSP


	try {
		// Deserialize the ScriptModule from a file using torch::jit::load().
		module = torch::jit::load("C:/Users/Ricky/projects/music/fm_synth_libtorch/traced_ssssm/traced_model.pt");


	}
	catch (const c10::Error& e) {
		std::cerr << "error loading the model\n";
	}


	std::cout << "Loaded model\n";


}


torch::Tensor NeuralNetwork::forward(const torch::Tensor& input)
{

	std::vector<torch::jit::IValue> my_input;
	my_input.push_back(input);


	torch::jit::IValue out = module.forward(my_input);
	return out.toTensor();
}

torch::jit::IValue NeuralNetwork::forward(torch::jit::IValue input)
{

	std::vector<torch::jit::IValue> my_input;
	my_input.push_back(input);

	return module.forward(my_input);
}