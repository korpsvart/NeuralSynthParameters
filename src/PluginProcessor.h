/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "NeuralNetwork.h"
#include <foleys_gui_magic/foleys_gui_magic.h>



//==============================================================================
/**
*/
class FMPluginProcessor  : public foleys::MagicProcessor, juce::AudioProcessorValueTreeState::Listener
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    FMPluginProcessor();
    ~FMPluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================

    void parameterChanged(const juce::String& parameterID, float newValue) override;



    /** add some midi to be played at the sent sample offset*/
    void addMidi(juce::MidiMessage msg, int sampleOffset);

private:

    //Synth variables

    std::vector<juce::Synthesiser*> synths;

    juce::Synthesiser* synth;

    //Parameters

    juce::AudioProcessorValueTreeState apvts;



    //Variables for neural part
    NeuralNetwork nn;


    //Loaded audio data
    //We must store this as a field because if we use from_blob
    //on this data the pointer to the original data must not be removed!
    std::vector<float> audioData;


    //Functions for handling tensors, audio files
    //and inference


    juce::AudioBuffer<float> loadAudioIntoBuffer(const juce::File& audioFile);

    //Convert audio buffer to tensor
    torch::Tensor audioBufferToTensor(const juce::AudioBuffer<float>& audioBuffer);

    //Convert the audio tensor to a tensor dict (format needed for the network input)
    torch::jit::IValue createInputDict(torch::Tensor& audioTensor);

    torch::Dict<torch::IValue, torch::IValue> getOutputDict(torch::jit::IValue& inputDict);

    torch::Dict<torch::IValue, torch::IValue> estimateSynthParams(const juce::AudioBuffer<float>& audioBuffer);


    //File functions
    void loadFile();


    void printOutputTensorEntry(torch::IValue key, torch::Dict<torch::IValue, torch::IValue>& dict); //For debugging
    void printSynthParamsDict(torch::Dict<torch::IValue, torch::IValue> synthParamsDict); //For debugging


    //Params functions

    juce::AudioProcessorValueTreeState::ParameterLayout createParams(); //to initialize the parameters

    void updateAllParameters(torch::Dict<torch::IValue, torch::IValue>); //Update all parameters from tensor


    /** stores messages added from the addMidi function*/
    juce::MidiBuffer midiToProcess;

    const std::vector<std::string> FMPluginProcessor::parameterNames = {
    "PEAK_A_1", "AT_A_1", "DE_A_1", "SU_A_1", "RE_A_1",
    "PEAK_A_2", "AT_A_2", "DE_A_2", "SU_A_2", "RE_A_2",
    "CUT_FLOOR", "PEAK_C", "AT_C", "DE_C", "SU_C", "RE_C",
    "OSC_MIX_1", "OSC_MIX_2", "F0_MULT", "Q_FILT",
    "LFO_RATE", "LFO_LEVEL", "MD_DELAY", "MD_DEPTH", "MD_MIX",
    "REV_GAIN", "REV_DEC"
    };

    std::vector<std::string> split(const std::string& s, char delimiter);

    bool updatedADSRa1;
    bool updatedADSRa2;
    bool updatedADSRc;


    //File loading

    std::unique_ptr<juce::FileChooser> myChooser;
    bool fileUpdated = false;




      //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMPluginProcessor)
};
