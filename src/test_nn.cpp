#include "NeuralNetwork.h"
#include <iostream>
#include <torch/torch.h>
#include <torch/script.h>


torch::jit::IValue loadTensor(std::string filename);
std::vector<char> get_the_bytes(std::string filename);
void printOutputTensorEntry(torch::IValue key, torch::Dict<torch::IValue, torch::IValue>& dict);

int main()
{

	//Test torchscript-imported SSSSM-DDSP neural network
	NeuralNetwork nn;

	torch::jit::IValue example_input;
	torch::jit::IValue example_output_concat;
	torch::jit::IValue example_input_concat;


	//Test 1
	//Load example input tensor and corresponding output from python
	//for network validation

	try {
		// Deserialize the ScriptModule from a file using torch::jit::load()

		example_input_concat = loadTensor("C:/Users/Ricky/projects/music/fm_synth_libtorch/traced_ssssm/in.pt");
		example_output_concat = loadTensor("C:/Users/Ricky/projects/music/fm_synth_libtorch/traced_ssssm/out.pt");
	}
	catch (const c10::Error& e) {
		std::cerr << "error loading tensors\n";
	}


	//Test model and compare result with the one obtained in Python
	torch::Tensor audio = example_input_concat.toGenericDict().at("audio").toTensor();

	auto x = torch::Dict<std::string, torch::Tensor>();
	x.insert("audio", audio);
	torch::jit::IValue input = torch::ivalue::from(x);


	torch::jit::IValue localOut = nn.forward(input);


	//Comparing the reference output tensor and the one obtained here
	torch::Dict<torch::IValue, torch::IValue> refOutputDict = example_output_concat.toGenericDict();
	torch::Dict<torch::IValue, torch::IValue> localOutputDict = localOut.toGenericDict();

	for (auto it = refOutputDict.begin(); it != refOutputDict.end(); ++it) {
		//Seems like there's no other easy way to iterate over...
		torch::IValue ref_Key = it->key();

		printOutputTensorEntry(ref_Key, refOutputDict);

		printOutputTensorEntry(ref_Key, localOutputDict);


	}




	return 0;

}

torch::jit::IValue createInputDict(torch::Tensor& audioTensor)
{
	auto x = torch::Dict<std::string, torch::Tensor>();
	x.insert("audio", audioTensor);
	torch::jit::IValue input = torch::ivalue::from(x);
	return input;
}



void printOutputTensorEntry(torch::IValue key, torch::Dict<torch::IValue, torch::IValue> &dict)
{
	std::string ref_key_str = key.toStringRef();
	torch::Tensor ref_tensor_value = dict.at(ref_key_str).toTensor();

	ref_tensor_value = ref_tensor_value.squeeze();

	std::cout << "Key: " << ref_key_str << ", Value in tensor: " << ref_tensor_value << std::endl;

	std::cout << "Test indexing and float conversion" << std::endl;
	if (ref_tensor_value.dim()>0)
	{

		float val1 = *ref_tensor_value.index({ 0 }).data<float>();
		float val2 = *ref_tensor_value.index({1 }).data<float>();

		std::cout << val1 << std::endl;
		std::cout << val2 << std::endl;
	}
	else {
		float val1 = *ref_tensor_value.data<float>();
		std::cout << val1 << std::endl;
	}
}

std::vector<char> get_the_bytes(std::string filename) {
	std::ifstream input(filename, std::ios::binary);
	std::vector<char> bytes(
		(std::istreambuf_iterator<char>(input)),
		(std::istreambuf_iterator<char>()));

	input.close();
	return bytes;
}


torch::jit::IValue loadTensor(std::string filename) {
	std::vector<char> f = get_the_bytes(filename);
	torch::IValue x = torch::pickle_load(f);
	//std::cout << "[cpp] my_tensor: " << my_tensor << std::endl;
	return x;
}
