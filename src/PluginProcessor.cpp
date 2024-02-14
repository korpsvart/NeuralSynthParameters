/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
//#include "PluginEditor.h"
#include "SynthSound.h"
#include "SynthVoice.h"
#include <build/juce_binarydata_fm-torchknob_data/JuceLibraryCode/BinaryData.h>


//==============================================================================
FMPluginProcessor::FMPluginProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : foleys::MagicProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
,  nn{}, apvts(*this, nullptr, "Parameters", FMPluginProcessor::createParams())//constructor of the audio components
{


    //Create synth voices

    //add voicesNumber voices to the synth (allows voicesNumber MIDI notes to be played at the same time)

    synth = new juce::Synthesiser();
    synth->addSound(new SynthSound());

    for (size_t voice = 0; voice < 8; voice++)
    {
        synth->addVoice(new SynthVoice(&apvts));

    }


    apvts.addParameterListener("AT_A_1", this);
    apvts.addParameterListener("DE_A_1", this);
    apvts.addParameterListener("SU_A_1", this);
    apvts.addParameterListener("RE_A_1", this);


    apvts.addParameterListener("AT_A_2", this);
    apvts.addParameterListener("DE_A_2", this);
    apvts.addParameterListener("SU_A_2", this);
    apvts.addParameterListener("RE_A_2", this);


    apvts.addParameterListener("AT_C", this);
    apvts.addParameterListener("DE_C", this);
    apvts.addParameterListener("SU_C", this);
    apvts.addParameterListener("RE_C", this);

    apvts.addParameterListener("REV_GAIN", this);
    apvts.addParameterListener("REV_DEC", this);

    myChooser = std::make_unique<juce::FileChooser>("Select an audio file to estimate the synthesizer parameters...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav");


    magicState.setGuiValueTree(BinaryData::magic_xml, BinaryData::magic_xmlSize); //load XML

    //the JUCE_MODAL_LOOPS_PERMITTED=1 definition must be specified for browseForFileToOpen to work
    magicState.addTrigger("loadFile", [&] { loadFile(); });






}

FMPluginProcessor::~FMPluginProcessor()
{
}

//==============================================================================
const juce::String FMPluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FMPluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FMPluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FMPluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FMPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FMPluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FMPluginProcessor::getCurrentProgram()
{
    return 0;
}

void FMPluginProcessor::setCurrentProgram (int index)
{
}

const juce::String FMPluginProcessor::getProgramName (int index)
{
    return {};
}

void FMPluginProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FMPluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..


    //Prepare the synths
    for (int i = 0; i < synth->getNumVoices(); i++)
    {
        if (auto voice = dynamic_cast<SynthVoice*>(synth->getVoice(i)))
        {
            voice->prepareToPlay(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
        }

    }
    synth->setCurrentPlaybackSampleRate(sampleRate);




}

void FMPluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.


    //Clear synths
    synth->clearSounds();
    synth->clearVoices();

    free(synth);
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FMPluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FMPluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    //////////// 
    // deal with MIDI 

     // transfer any pending notes into the midi messages and 
    // clear pending - these messages come from the addMidi function
    // which the UI might call to send notes from the piano widget
    if (midiToProcess.getNumEvents() > 0){
      midiMessages.addEvents(midiToProcess, midiToProcess.getFirstEventTime(), midiToProcess.getLastEventTime()+1, 0);
      midiToProcess.clear();
    }



    magicState.processMidiBuffer(midiMessages, buffer.getNumSamples());

    if (fileUpdated)
    {

        fileUpdated = false;
        myChooser->getResult();

        //Perform estimation of synth params
        juce::File audioFile = myChooser->getResult();
        juce::AudioBuffer<float> audioBuffer = loadAudioIntoBuffer(audioFile);

        torch::Dict<torch::IValue, torch::IValue> synthParams = estimateSynthParams(audioBuffer);

        updateAllParameters(synthParams);
    }

    if (updatedADSRa1)
    {
        updatedADSRa1 = false;
        for (int i = 0; i < synth->getNumVoices(); ++i)
        {
            auto& attack = *apvts.getRawParameterValue("AT_A_1");
            auto& decay = *apvts.getRawParameterValue("DE_A_1");
            auto& sustain = *apvts.getRawParameterValue("SU_A_1");
            auto& release = *apvts.getRawParameterValue("RE_A_1");
            if (auto voice = dynamic_cast<SynthVoice*>(synth->getVoice(i)))
            {
                voice->updateADSRA1(attack.load(), decay.load(), sustain.load(), release.load());

            }

        }
    }

    if (updatedADSRa2)
    {
        updatedADSRa2 = false;
        for (int i = 0; i < synth->getNumVoices(); ++i)
        {
            auto& attack = *apvts.getRawParameterValue("AT_A_2");
            auto& decay = *apvts.getRawParameterValue("DE_A_2");
            auto& sustain = *apvts.getRawParameterValue("SU_A_2");
            auto& release = *apvts.getRawParameterValue("RE_A_2");
            if (auto voice = dynamic_cast<SynthVoice*>(synth->getVoice(i)))
            {
                voice->updateADSRA2(attack.load(), decay.load(), sustain.load(), release.load());

            }

        }
    }
  

    if (updatedADSRc)
    {
        updatedADSRc = false;
        for (int i = 0; i < synth->getNumVoices(); ++i)
        {
            auto& attack = *apvts.getRawParameterValue("AT_C");
            auto& decay = *apvts.getRawParameterValue("DE_C");
            auto& sustain = *apvts.getRawParameterValue("SU_C");
            auto& release = *apvts.getRawParameterValue("RE_C");
            if (auto voice = dynamic_cast<SynthVoice*>(synth->getVoice(i)))
            {
                voice->updateADSRc(attack.load(), decay.load(), sustain.load(), release.load());

            }

        }

    }

    if (updatedReverb)
    {
        updatedReverb = false;
        for (int i = 0; i < synth->getNumVoices(); ++i)
        {
            if (auto voice = dynamic_cast<SynthVoice*>(synth->getVoice(i)))
            {
                voice->updateReverb();

            }
        }
    }

    synth->renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    midiMessages.clear();
 
}

//==============================================================================


void FMPluginProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{

   
    std::vector<std::string> tokens = split(parameterID.toStdString(), '_');
    if (tokens[0]=="REV")
    {
        updatedReverb = true;
    }
    else {
        if (tokens[1] == "A")
        {
            if (tokens[2] == "1")
            {
                updatedADSRa1 = true;
            }
            else
            {
                updatedADSRa2 = true;
            }
        }
        else
        {
            updatedADSRc = true;
        }
    }




}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMPluginProcessor();
}


juce::AudioBuffer<float> FMPluginProcessor::loadAudioIntoBuffer(const juce::File& audioFile)
{

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> audioReader(formatManager.createReaderFor(audioFile));

    int numChannels = audioReader->numChannels; //We check this but it should always be mono
    int numSamples = audioReader->lengthInSamples;

    juce::AudioBuffer<float> audioBuffer(numChannels, numSamples);

    audioReader->read(&audioBuffer, 0, numSamples, 0, true, true);

    return audioBuffer;

}
torch::Tensor FMPluginProcessor::audioBufferToTensor(const juce::AudioBuffer<float>& audioBuffer)
{
    int numChannels = audioBuffer.getNumChannels(); //We should check the audio is mono
    DBG(numChannels);
    int numSamples = audioBuffer.getNumSamples();

    //For now just read channel 0
    const float* channelData = audioBuffer.getReadPointer(0);
    audioData.clear();
    audioData.insert(audioData.end(), channelData, channelData + numSamples);

    //Create the audio tensor
    torch::Tensor tensor = torch::from_blob(audioData.data(), {numChannels, numSamples}, torch::kFloat32);

    return tensor;
}
torch::jit::IValue FMPluginProcessor::createInputDict(torch::Tensor& audioTensor)
{
    auto x = torch::Dict<std::string, torch::Tensor>();
    x.insert("audio", audioTensor);
    torch::jit::IValue input = torch::ivalue::from(x);
    return input;
}
torch::Dict<torch::IValue, torch::IValue> FMPluginProcessor::getOutputDict(torch::jit::IValue& inputDict)
{

    torch::jit::IValue output = nn.forward(inputDict); //Inference

    torch::Dict<torch::IValue, torch::IValue> outputDict = output.toGenericDict(); //Convert to Dict

    return outputDict;


}
torch::Dict<torch::IValue, torch::IValue> FMPluginProcessor::estimateSynthParams(const juce::AudioBuffer<float>& audioBuffer)
{
    torch::Tensor inputTensor = audioBufferToTensor(audioBuffer);

    DBG(inputTensor.size(0));
    DBG(inputTensor.size(1));

    torch::jit::IValue inputDict = createInputDict(inputTensor);

    return getOutputDict(inputDict);
    
}
void FMPluginProcessor::loadFile()
{
    fileUpdated = myChooser->browseForFileToOpen(); //returns true if a file was chosen

}
void FMPluginProcessor::printOutputTensorEntry(torch::IValue key, torch::Dict<torch::IValue, torch::IValue>& dict)
{
    std::string ref_key_str = key.toStringRef();
    torch::Tensor ref_tensor_value = dict.at(ref_key_str).toTensor();

    float float_value = *ref_tensor_value.data<float>();
    const float* dataPtr = ref_tensor_value.data_ptr<float>();

    std::string debugString = "Key: " + ref_key_str + ", Value in tensor: " + std::to_string(*dataPtr);
    DBG(debugString);
}
void FMPluginProcessor::printSynthParamsDict(torch::Dict<torch::IValue, torch::IValue> synthParamsDict)
{
    for (auto it = synthParamsDict.begin(); it != synthParamsDict.end(); ++it) {
        //Seems like there's no other easy way to iterate over...
        torch::IValue ref_Key = it->key();

        printOutputTensorEntry(ref_Key, synthParamsDict);
    }

}


void FMPluginProcessor::addMidi(juce::MidiMessage msg, int sampleOffset)
{
  midiToProcess.addEvent(msg, sampleOffset);
}

juce::AudioProcessorValueTreeState::ParameterLayout FMPluginProcessor::createParams()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ADSR 1 Params
    auto peak_a_1 = std::make_unique<juce::AudioParameterFloat>("PEAK_A_1", "Attack amplitude (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto at_a_1 = std::make_unique<juce::AudioParameterFloat>("AT_A_1", "Attack time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto de_a_1 = std::make_unique<juce::AudioParameterFloat>("DE_A_1", "Decay time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto su_a_1 = std::make_unique<juce::AudioParameterFloat>("SU_A_1", "Sustain level (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto re_a_1 = std::make_unique<juce::AudioParameterFloat>("RE_A_1", "Release time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto groupADSR1 = std::make_unique<juce::AudioProcessorParameterGroup>("adsr1", "ADSR1", "|");
    groupADSR1->addChild(std::move(peak_a_1));
    groupADSR1->addChild(std::move(at_a_1));
    groupADSR1->addChild(std::move(de_a_1));
    groupADSR1->addChild(std::move(su_a_1));
    groupADSR1->addChild(std::move(re_a_1));
    layout.add(std::move(groupADSR1));

    // ADSR 2 Params
    auto peak_a_2 = std::make_unique<juce::AudioParameterFloat>("PEAK_A_2", "Attack amplitude (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto at_a_2 = std::make_unique<juce::AudioParameterFloat>("AT_A_2", "Attack time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto de_a_2 = std::make_unique<juce::AudioParameterFloat>("DE_A_2", "Decay time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto su_a_2 = std::make_unique<juce::AudioParameterFloat>("SU_A_2", "Sustain level (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto re_a_2 = std::make_unique<juce::AudioParameterFloat>("RE_A_2", "Release time (ADSR 1)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto groupADSR2 = std::make_unique<juce::AudioProcessorParameterGroup>("adsr2", "ADSR2", "|");
    groupADSR2->addChild(std::move(peak_a_2));
    groupADSR2->addChild(std::move(at_a_2));
    groupADSR2->addChild(std::move(de_a_2));
    groupADSR2->addChild(std::move(su_a_2));
    groupADSR2->addChild(std::move(re_a_2));
    layout.add(std::move(groupADSR2));

    float nyquist = getSampleRate() / 2;
    if (nyquist == 0) nyquist = 20000.0; //Sometimes if called at the start it will be zero

    // ADSR C Params
    auto cut_floor = std::make_unique<juce::AudioParameterFloat>("CUT_FLOOR", "Minimum cutoff (ADSR C)",
        juce::NormalisableRange<float> {30.0f, nyquist, 1.0f}, 0.01f);

    auto peak_c = std::make_unique<juce::AudioParameterFloat>("PEAK_C", "Attack amplitude (ADSR C)",
        juce::NormalisableRange<float> {30.0f, nyquist, 1.0f}, 0.01f);

    auto at_c = std::make_unique<juce::AudioParameterFloat>("AT_C", "Attack time (ADSR C)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto de_c = std::make_unique<juce::AudioParameterFloat>("DE_C", "Decay time (ADSR C)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto su_c = std::make_unique<juce::AudioParameterFloat>("SU_C", "Sustain level (ADSR C)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto re_c = std::make_unique<juce::AudioParameterFloat>("RE_C", "Release time (ADSR C)",
        juce::NormalisableRange<float> {0.01f, 4.0f, 0.001f}, 0.01f);

    auto groupADSRC = std::make_unique<juce::AudioProcessorParameterGroup>("adsrc", "ADSRC", "|");
    groupADSRC->addChild(std::move(cut_floor));
    groupADSRC->addChild(std::move(peak_c));
    groupADSRC->addChild(std::move(at_c));
    groupADSRC->addChild(std::move(de_c));
    groupADSRC->addChild(std::move(su_c));
    groupADSRC->addChild(std::move(re_c));
    layout.add(std::move(groupADSRC));

    // Oscillator params
    auto osc_mix_1 = std::make_unique<juce::AudioParameterFloat>("M_OSC_1", "Wavetype mix (Saw-Square)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto osc_mix_2 = std::make_unique<juce::AudioParameterFloat>("M_OSC_2", "Wavetype mix (Saw-Square)",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 0.01f);

    auto f0_mult = std::make_unique<juce::AudioParameterFloat>("F0_MULT", "Frequency Ratio",
        juce::NormalisableRange<float> {1.0f, 8.0f, 0.1f}, 3.0f);

    auto groupOscillator = std::make_unique<juce::AudioProcessorParameterGroup>("oscillator", "Oscillator", "|");
    groupOscillator->addChild(std::move(osc_mix_1));
    groupOscillator->addChild(std::move(osc_mix_2));
    groupOscillator->addChild(std::move(f0_mult));
    layout.add(std::move(groupOscillator));

    // Filter params
    auto q_filt = std::make_unique<juce::AudioParameterFloat>("Q_FILT", "Q Factor",
        juce::NormalisableRange<float> {0.02f, 2.0f, 0.001f}, 0.01f);
    auto groupFilter = std::make_unique<juce::AudioProcessorParameterGroup>("filter", "Filter", "|");
    groupFilter->addChild(std::move(q_filt));
    layout.add(std::move(groupFilter));

    // LFO params
    //(These are actually not inferred by the model but fixed, but we will include them)
    auto lfo_rate = std::make_unique<juce::AudioParameterFloat>("LFO_RATE", "LFO Rate",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto lfo_level = std::make_unique<juce::AudioParameterFloat>("LFO_LEVEL", "LFO Level",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto groupLFO = std::make_unique<juce::AudioProcessorParameterGroup>("lfo", "LFO", "|");
    groupLFO->addChild(std::move(lfo_rate));
    groupLFO->addChild(std::move(lfo_level));
    layout.add(std::move(groupLFO));

    // Chorus (modulated delay) params
    auto md_delay = std::make_unique<juce::AudioParameterFloat>("MD_DELAY", "Chorus Modulation Delay",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    // Fixed, not trained
    auto md_depth = std::make_unique<juce::AudioParameterFloat>("MD_DEPTH", "Chorus Modulation Depth",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto md_mix = std::make_unique<juce::AudioParameterFloat>("MD_MIX", "Chorus Mix",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto groupChorus = std::make_unique<juce::AudioProcessorParameterGroup>("chorus", "Chorus", "|");
    groupChorus->addChild(std::move(md_delay));
    groupChorus->addChild(std::move(md_depth));
    groupChorus->addChild(std::move(md_mix));
    layout.add(std::move(groupChorus));

    // Reverb params
    auto rev_gain = std::make_unique<juce::AudioParameterFloat>("REV_GAIN", "Reverb Gain",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto rev_dec = std::make_unique<juce::AudioParameterFloat>("REV_DEC", "Reverb Decay",
        juce::NormalisableRange<float> {0.01f, 1.0f, 0.001f}, 1.0f);

    auto groupReverb = std::make_unique<juce::AudioProcessorParameterGroup>("reverb", "Reverb", "|");
    groupReverb->addChild(std::move(rev_gain));
    groupReverb->addChild(std::move(rev_dec));
    layout.add(std::move(groupReverb));

    return layout;
}

void FMPluginProcessor::updateAllParameters(torch::Dict<torch::IValue, torch::IValue> inputDict)
{
    for (auto item = inputDict.begin(); item != inputDict.end(); ++item) 
    {
        // Extract key and value
        torch::IValue ref_Key = item->key();
        std::string ref_key_str = ref_Key.toStringRef();
        torch::Tensor ref_tensor_value = inputDict.at(ref_key_str).toTensor();

        ref_tensor_value = ref_tensor_value.squeeze(); //remove unnecessary dimensions

        //skip some fixed parameters
        if (ref_key_str == "BFRQ" || ref_key_str == "NOISE_A" || ref_key_str == "NOISE_C" || ref_key_str == "NOTE_OFF" || ref_key_str == "AMP_FLOOR") continue;


        DBG("Key:" + ref_key_str + ", values:");


        if (ref_tensor_value.dim() > 0) //tensor is a list
        {

            float val1 = *ref_tensor_value.index({ 0 }).data<float>();
            float val2 = *ref_tensor_value.index({ 1 }).data<float>();

            DBG(val1 << "," << val2);


            //All params in tensor are normalized [0,1]. The remapping is done automatically by the RangedAudioParameter class

            juce::RangedAudioParameter *p1=  magicState.getParameter(ref_key_str + "_1");
            float denormalizedValue1 = p1->convertFrom0to1(val1);
            p1->setValueNotifyingHost(val1);


            juce::RangedAudioParameter* p2 = magicState.getParameter(ref_key_str + "_2");
            float denormalizedValue2 = p2->convertFrom0to1(val2);
            p2->setValueNotifyingHost(val2);

            DBG("Denormalized: " << denormalizedValue1 << "," << denormalizedValue2);

        }
        else  //tensor is a single float element
        {
            float val1 = *ref_tensor_value.data<float>();

            DBG(val1);

            juce::RangedAudioParameter* p1 = magicState.getParameter(ref_key_str);
            float denormalizedValue1 = p1->convertFrom0to1(val1);
            p1->setValueNotifyingHost(val1);

            DBG("Denormalized: " << denormalizedValue1);
        }


    }
}

std::vector<std::string> FMPluginProcessor::split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}




