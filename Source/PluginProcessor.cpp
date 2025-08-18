/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CustomEQAudioProcessor::CustomEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

CustomEQAudioProcessor::~CustomEQAudioProcessor()
{
}

//==============================================================================
const juce::String CustomEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CustomEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CustomEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CustomEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CustomEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int CustomEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CustomEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CustomEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String CustomEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void CustomEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void CustomEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void CustomEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool CustomEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void CustomEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
}

//==============================================================================
bool CustomEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* CustomEQAudioProcessor::createEditor()
{
    //return new CustomEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void CustomEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void CustomEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

juce::AudioProcessorValueTreeState::ParameterLayout
    CustomEQAudioProcessor::createParameterLayout() 
{
    // 3 EQ bands: Low, High, Peak
    // Low and High, we controller FQ cutoff and slop
    // Peak -> center Frequency, Gain, and Quailty (how low/wide the cut)

    // AudioProcessorParameter -> generic audio interface for all audio parameter formats
    // that different plugin units used (VST, AUv3, RTAS, AAX, etc.)
    // AUdioParameterFLoat -> Parameters with a ragne of values 

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "LowCut Freq", 
        "LowCut Freq", 
        juce::NormalisableRange<float>(20.f, 2000.f, 1.f, 1.f), 
        20.f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "HighCut Freq",
        "HighCut Freq",
        juce::NormalisableRange<float>(20.f, 2000.f, 1.f, 1.f),
        20000.f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "HighCut Freq",
        "HighCut Freq",
        juce::NormalisableRange<float>(20.f, 2000.f, 1.f, 1.f), // range start, range end, interval, skew -> when moving knob how much is knob dedicated to controller low/high
        750.f // starting value
    ));


    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Gain",
        "Peak Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f), // controls in decibel
        0.0f// no gain
    ));

    // quality control of how narrow or wide the Q value is for the band
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Quality ",
        "Peak Quality",
        juce::NormalisableRange<float>(-1.f,10.f, 0.05f, 1.f), // controls in decibel, steps -> 0.05, 0.01, etc.
        1.f// no gain
    ));

    // low cut and high cut steepness, expressed in dB per octav and are muiltiples of 6 or 12s. because we have a choice, we use the AudioParameterChoice object 

    juce::StringArray stringArray;
    for (int i = 0; i < 4; i++)
    {
        juce::String str; 
        str << (12 + i * 12);
        str << " db/Oct";
        stringArray.add(str);
    }

    stringArray.add("96 db/Oct");

    // default slop is 12
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));


    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CustomEQAudioProcessor();
}
