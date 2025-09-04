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

    // we must prepare our filters before we use them, we pass a ProcessesSpecObject to the chains which is passed to the link of each chain
    juce::dsp::ProcessSpec spec; 
    spec.maximumBlockSize = samplesPerBlock;

    spec.numChannels = 1; // mono is 1 chain

    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    // coefficient 
    auto chainSettings = getChainSettings(apvts);

    updatePeakFilter(chainSettings);


    // helper functions for IIR
    // inside the implementation
    /*
        it will create 1 IIR coefficient for every 2 order, we need to create the right amoutn of objects for teh right slope
        or choices: 12, 24, 36, 48, 96. So if we needed to create a 12 db/oct, we need to give an order of 2, if i = 1, 24, we need 2 coefficient

        so our orders are 2, 4 ,6, 8 for slope choice 0, 1, 2, 3 for 12, 24 ,36 ,48 db/oct

        but we also have 96 db/oct, with that logic, the orders will have to be 2, 4, 6, and 16 for 5 choices
    */
    int order = 0;
    switch (chainSettings.lowCutSlope)
    {
        case Slope_12: order = 2;  break;
        case Slope_24: order = 4;  break;
        case Slope_36: order = 6;  break;
        case Slope_48: order = 8;  break;
        case Slope_96: order = 16; break;
    }

    auto lowCutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq,
        getSampleRate(),
        order); // 0, 1, 2 ,3, 4? and double it to get us 2 4 6 8, and 10? should be 16

    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, lowCutCoefficients, chainSettings.lowCutSlope);

    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(rightLowCut, lowCutCoefficients, chainSettings.lowCutSlope);


    // high cut, same thing
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        chainSettings.highCutFreq,
        getSampleRate(),
        order
    );

    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);

    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
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

    // updating parameter BEFORE audio processing
    auto chainSettings = getChainSettings(apvts);
    
    updatePeakFilter(chainSettings);


    // we a context for the processing block to run the links in the chain
    // we must provide an audio block for the context. we need the channels

    juce::dsp::AudioBlock<float> block(buffer);


    int order = 0;
    switch (chainSettings.lowCutSlope)
    {
    case Slope_12: order = 2;  break;
    case Slope_24: order = 4;  break;
    case Slope_36: order = 6;  break;
    case Slope_48: order = 8;  break;
    case Slope_96: order = 16; break;
    }

    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(
        chainSettings.lowCutFreq,
        getSampleRate(),
        order); // 0, 1, 2 ,3, 4? and double it to get us 2 4 6 8, and 10? should be 16

    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings.lowCutSlope);

    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);

    // high cut, same thing
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(
        chainSettings.highCutFreq,
        getSampleRate(),
        order
    );

    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);

    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);


    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChain.process(leftContext);
    rightChain.process(rightContext);
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

// getting params
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;

    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load(); // units in range we care about, when we get it
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load(); 
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load(); 
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load(); 
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load(); 
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    
    return settings;
}

void CustomEQAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements) {
    *old = *replacements;
}

void CustomEQAudioProcessor::updatePeakFilter(const ChainSettings& chainSettings) 
{
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(),
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));

    // setting coefficients 
    updateCoefficients(leftChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
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
        juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 1.f), 
        20.f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "HighCut Freq",
        "HighCut Freq",
        juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 1.f),
        20000.f
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Freq",
        "Peak Freq",
        juce::NormalisableRange<float>(10.f, 20000.f, 1.f, 0.25f), // range start, range end, interval, skew -> when moving knob how much is knob dedicated to controller low/high
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
        "Peak Quality",
        "Peak Quality",
        juce::NormalisableRange<float>(0.1f,10.f, 0.05f, 1.f), // controls in decibel, steps -> 0.05, 0.01, etc.
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
