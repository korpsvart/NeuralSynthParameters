
#pragma once
//#include <JuceHeader.h>
#include "SynthSound.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>

class SynthVoice : public juce::SynthesiserVoice
{
public:

    SynthVoice(juce::AudioProcessorValueTreeState* apvts);

    

    bool canPlaySound(juce::SynthesiserSound*) override;

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;

    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock, int outputChannelsNumber);


    void updateADSRA1(float attack, float decay, float sustain, float release);

    void updateADSRA2(float attack, float decay, float sustain, float release);

    void updateADSRc(float attack, float decay, float sustain, float release);


private:


    //We use oscillators only to compute the harmonic components sine functions
    //because they can employ a lookup table to improve performance
    std::vector<juce::dsp::Oscillator<float>> osc1; //24 Oscillators (for each harmonic) for Synth 1
    std::vector<juce::dsp::Oscillator<float>> osc2; //24 Oscillators (for each harmonic) for synth 2

    juce::ADSR adsrOsc1;
    juce::ADSR adsrOsc2;
    juce::ADSR adsrC; //ADSR for cutoff frequency

    juce::ADSR::Parameters adsrOsc1Params{};
    juce::ADSR::Parameters adsrOsc2Params{};
    juce::ADSR::Parameters adsrCParams{};

    bool isPrepared = false;

    juce::AudioProcessorValueTreeState* apvts;


    float computeOscOutput(float inputSample);

    //Precomputed harmonic amplitudes for efficiency
    std::vector<float> harmonicsSquare;
    std::vector<float> harmonicsSaw;

    //Oscillator variables

    float currentFrequency;




    

};
