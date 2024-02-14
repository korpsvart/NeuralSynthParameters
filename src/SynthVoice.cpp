

#include "SynthVoice.h"
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>


SynthVoice::SynthVoice(juce::AudioProcessorValueTreeState* apvts)
{

    this->apvts = apvts;

    currentFrequency = 440; //just for init

    float sumSaw = 0.0f;
    float sumSquare = 0.0f;

    //Initialize 
    for (int k = 1; k <= 24; k++)
    {
        //Compute the harmonics amplitude
        float saw = (1.0 / k) * 2 / juce::MathConstants<float>::pi;
        int odd = (k % 2 != 0); //Only odd harmonics for the square wave
        float square = (1.0 / k) * 4 / juce::MathConstants<float>::pi * odd;


        harmonicsSaw.push_back(saw);
        harmonicsSquare.push_back(square);

        sumSaw += saw;
        sumSquare += square;

        //Create the oscillators
        osc1.push_back(juce::dsp::Oscillator<float>{ [](float x) { return std::sin(x); }, 128 });
        osc2.push_back(juce::dsp::Oscillator<float>{ [](float x) { return std::sin(x); }, 128 });
    }

    //Normalize the amplitudes
    for (float& value : harmonicsSaw)
    {
        value /= sumSaw;
    }

    for (float& value : harmonicsSquare)
    {
        value /= sumSquare;
    }


    //Initialize the variables for convolutional reverb
    //noise.setSize(1, irLength);
    //time.setSize(1, irLength);
    for (int i = 0; i < irLength; ++i) //skip first s
        noise.setSample(0, i, juce::Random::getSystemRandom().nextFloat() * 2 - 1); // To have [-1, 1) range
    noise.setSample(0, 0, 0.0); //set to zero first sample
    for (int i = 0; i < irLength; ++i)
        time.setSample(0, i, static_cast<float>(i) / getSampleRate()); //time ramp
        //time.setSample(0, i, static_cast<float>(i) / static_cast<float>(irLength)); //time ramp


    //ir.setSize(1, irLength);


}



bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<juce::SynthesiserSound*>(sound) != nullptr;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition)
{

    currentFrequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    float f0_mult = *apvts->getRawParameterValue("F0_MULT");

    //Set the oscillator frequencies
    for (int i = 1; i <= 24; ++i)
    {
        osc1[i-1].setFrequency(i * currentFrequency);
        osc2[i-1].setFrequency(i * currentFrequency * f0_mult);
    }

    //Start the attack phase of the ADSR envelopes
    adsrOsc1.noteOn();
    adsrOsc2.noteOn();
    adsrC.noteOn();


}

void SynthVoice::stopNote(float velocity, bool allowTailOff)
{

    adsrOsc1.noteOff();
    adsrOsc2.noteOff();
    adsrC.noteOff();


    bool active = adsrOsc1.isActive() || adsrOsc2.isActive();


    if (!allowTailOff || !active)
        clearCurrentNote();
}

void SynthVoice::pitchWheelMoved(int newPitchWheelValue)
{
}

void SynthVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{

    jassert(isPrepared);


    if (!isVoiceActive())
        return;



    while (--numSamples >= 0)
    {
        auto currentSample = computeOscOutput(0);

        if (abs(currentSample) > 1) DBG("Warning clipping");

        for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
            outputBuffer.addSample(i, startSample, currentSample);

        startSample++;
    }


    //applyReverb(outputBuffer);


    bool active = adsrOsc1.isActive() || adsrOsc2.isActive();


    if (!active)
        clearCurrentNote();

}


double lowpass(double freq, double cutoff, double q) {

    //The lowpass filter is implemented in the diff synth simply by multiplying the harmonics
    //with a frequency response. This is done because IIR would be inefficient in the training process.
    //While we could use IIR filters here instead, we try to replicate the exact behaviour for accuracy.

    //Freq: the frequency of the harmonic
    //Cutoff: the cutoff frequency
    //q: the q factor

    double r = freq / cutoff;
    return 1 / std::sqrt(std::pow( (1 - std::pow(r, 2.0)), 2.0) + std::pow(r / q, 2.0));

}


float SynthVoice::computeOscOutput(float inputSample)
{
    //Compute one single output sample from the first "stage" (oscillators sum) + second "stage" (filter)

    float output = 0.0;
    //Outputs from the two synthesizers

    float oscMix1 = *apvts->getRawParameterValue("M_OSC_1");
    float oscMix2 = *apvts->getRawParameterValue("M_OSC_2");

    //Retrieve the current cutoff frequency
    //This is controlled by the ADSR. The ADSR module in juce doesn't allow changing the floor
    //and peak values, so we must rescale them manually.
    float adsrCSample = adsrC.getNextSample();
    //refactor this later to avoid retrieving them every time
    float cutFloor = *apvts->getRawParameterValue("CUT_FLOOR");
    float cutPeak = *apvts->getRawParameterValue("PEAK_C");
    float qFilt = *apvts->getRawParameterValue("Q_FILT");
    float currentCutoffFrequency = adsrCSample * (cutPeak - cutFloor) + cutFloor;

    for (size_t k = 1; k <= 24; k++)
    {
        //We can do it in one single cycle (doesn't matter if we sum here or later)

        //If above nyquist frequency stop
        if ((k * currentFrequency) > (getSampleRate() / 2)) break;

        //These are the same for both oscs
        float squareAmp = harmonicsSquare.at(k - 1);
        float sawAmp = harmonicsSaw.at(k - 1);
        float lowpassAmp = lowpass(currentFrequency * k, currentCutoffFrequency, qFilt);

        //These depends on the osc

        //OSC1
        float sineWave = osc1[k - 1].processSample(inputSample);
        float ampOsc = (1 - oscMix1) * sawAmp + oscMix1 * squareAmp;

        float ADSROsc = adsrOsc1.getNextSample(); //Compute adsr sample


        output += ADSROsc * ampOsc * sineWave *lowpassAmp;

        //OSC2

        sineWave = osc2[k - 1].processSample(inputSample);
        ampOsc = (1 - oscMix2) * sawAmp + oscMix2 * squareAmp;
        ADSROsc = adsrOsc2.getNextSample();

        output += ADSROsc * ampOsc * sineWave * lowpassAmp;

    }

    return output;

}



void SynthVoice::prepareToPlay(double sampleRate, int samplesPerBlock, int outputChannelsNumber)
{

    //Prepare oscillator (passing ProcessSpec)


    //Prepare ADSR

    adsrOsc1.setSampleRate(sampleRate);
    adsrOsc2.setSampleRate(sampleRate);
    adsrC.setSampleRate(sampleRate);


    
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = outputChannelsNumber;


    for (size_t i = 0; i < 24; i++)
    {
        osc1[i].prepare(spec);
        osc2[i].prepare(spec);
    }




    isPrepared = true;
}

void SynthVoice::updateADSRA1(float attack, float decay, float sustain, float release)
{
    //Scale the received values to have actuals seconds
    adsrOsc1Params.attack = attack*4;
    adsrOsc1Params.decay = decay*4;
    adsrOsc1Params.sustain = sustain;
    adsrOsc1Params.release = release*4;
    adsrOsc1.setParameters(adsrOsc1Params);

}

void SynthVoice::updateADSRA2(float attack, float decay, float sustain, float release)
{
    //Scale the received values to have actuals seconds
    adsrOsc2Params.attack = attack * 4;
    adsrOsc2Params.decay = decay * 4;
    adsrOsc2Params.sustain = sustain;
    adsrOsc2Params.release = release * 4;
    adsrOsc2.setParameters(adsrOsc2Params);
}

void SynthVoice::updateADSRc(float attack, float decay, float sustain, float release)
{
    //Scale the received values to have actuals seconds
    adsrCParams.attack = attack * 4;
    adsrCParams.decay = decay * 4;
    adsrCParams.sustain = sustain;
    adsrCParams.release = release * 4;
    adsrC.setParameters(adsrCParams);

}

void SynthVoice::applyReverb(const juce::AudioBuffer<float>& audio)
{

    juce::AudioSampleBuffer abfs(audio);
    juce::dsp::AudioBlock<float> block(abfs);
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);



}

void SynthVoice::updateReverb()
{

    //Recompute the impulse response

    float gain = *apvts->getRawParameterValue("REV_GAIN");
    float decay = *apvts->getRawParameterValue("REV_DEC");
    ir.setSize(1, irLength);
    for (int i = 0; i < irLength; ++i)
        ir.setSample(0, i, gain * std::exp(-decay * time.getSample(0, i)) * noise.getSample(0, i));


    //Update convolution object


    convolution.reset();
    convolution.loadImpulseResponse(std::move(ir), getSampleRate(), juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    convolution.prepare(spec);

}





