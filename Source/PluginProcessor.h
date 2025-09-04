/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Defining and extracting our parameters from the tree states
enum Slope 
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48,
    Slope_96
};

struct ChainSettings 
{
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    Slope lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
};

// help func to give us our parameters values
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//==============================================================================
/**
*/
class CustomEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    CustomEQAudioProcessor();
    ~CustomEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

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
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout 
        createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout()};

private:
    // so much of the audio is mono, but we are making a stereo VST, this is how we are going to fix that
    using Filter = juce::dsp::IIR::Filter<float>; 

    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter, Filter, Filter, Filter, Filter>; // basically how we are doing 12, 24, 36, etc. is we are just chaining the same filter over and over again

    // chain for mono 
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

    // need 2 mono chains for stereo
    MonoChain leftChain, rightChain;

    enum ChainPositions
    {
        LowCut,
        Peak,
        HighCut
    };


    void updatePeakFilter(const ChainSettings& chainSettings);
    using Coefficients = Filter::CoefficientsPtr;
    static void updateCoefficients(Coefficients& old, const Coefficients& replacements);

    template<int Index, typename ChainType, typename CoefficientType>
    void update(ChainType& chain, const CoefficientType& coefficients) {
        updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
        chain.template setBypassed<Index>(false);
    }

    template<typename ChainType, typename CoefficientType>
    void updateCutFilter(ChainType& leftLowCut, const CoefficientType& cutCoefficients, const Slope& lowCutSlope)
    {
        // 8 pos, bypass all
        leftLowCut.template setBypassed<0>(true);
        leftLowCut.template setBypassed<1>(true);
        leftLowCut.template setBypassed<2>(true);
        leftLowCut.template setBypassed<3>(true);
        leftLowCut.template setBypassed<4>(true);
        leftLowCut.template setBypassed<5>(true);
        leftLowCut.template setBypassed<6>(true);
        leftLowCut.template setBypassed<7>(true);
                            
        // order is 2* ish, helper will return 1 coefficient only
        // using case slope reversing so no more breaking
        switch (lowCutSlope)
        {
            case Slope_96: {
                update<7>(leftLowCut, cutCoefficients);
                update<6>(leftLowCut, cutCoefficients);
                update<5>(leftLowCut, cutCoefficients);
                update<4>(leftLowCut, cutCoefficients);
            }
            case Slope_48: {
                update<3>(leftLowCut, cutCoefficients);
            }
            case Slope_36: {
                update<2>(leftLowCut, cutCoefficients);
            }
            case Slope_24: {
                update<1>(leftLowCut, cutCoefficients);
            }
            case Slope_12: {
                update<0>(leftLowCut, cutCoefficients);
            }
        }
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomEQAudioProcessor)
};
