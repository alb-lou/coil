#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout CoilProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto id = [] (const char* s) { return ParameterID { s, 1 }; };
    auto f  = [&] (const char* i, const char* n, NormalisableRange<float> r, float d)
    { p.push_back (std::make_unique<AudioParameterFloat> (id (i), n, r, d)); };

    p.push_back (std::make_unique<AudioParameterChoice> (id ("model"), "Model",
                 StringArray { "Vintage", "Keys", "Hi-Gain", "Studio" }, 0));
    f ("decay",   "Decay",    { 0.3f, 8.0f, 0.01f, 0.5f }, 2.0f);
    f ("tension", "Tension",  { 0.f, 1.f, 0.001f }, 0.5f);
    f ("bright",  "Bright",   { 0.f, 1.f, 0.001f }, 0.5f);
    f ("boingy",  "Boingy",   { 0.f, 1.f, 0.001f }, 0.5f);
    f ("drive",   "Drive",    { 0.f, 1.f, 0.001f }, 0.2f);
    f ("eq1gain", "Low Gain", { -12.f, 12.f, 0.1f }, 0.f);
    f ("eq1freq", "Low Freq", { 40.f, 1000.f, 1.f, 0.5f }, 150.f);
    f ("eq1width","Low Width",{ 0.f, 1.f, 0.001f }, 0.5f);
    f ("eq2gain", "High Gain",{ -12.f, 12.f, 0.1f }, 0.f);
    f ("eq2freq", "High Freq",{ 1000.f, 16000.f, 1.f, 0.5f }, 5000.f);
    f ("eq2width","High Width",{ 0.f, 1.f, 0.001f }, 0.5f);
    f ("dry",     "Dry",      { 0.f, 100.f, 0.1f }, 100.f);
    f ("wet",     "Wet",      { 0.f, 100.f, 0.1f }, 35.f);
    f ("width",   "Wet Width",{ 0.f, 200.f, 0.1f }, 100.f);
    f ("pan",     "Wet Pan",  { -100.f, 100.f, 0.1f }, 0.f);
    return { p.begin(), p.end() };
}

CoilProcessor::CoilProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    auto g = [this] (const char* s) { return apvts.getRawParameterValue (s); };
    pModel = g ("model"); pDecay = g ("decay"); pTension = g ("tension"); pBright = g ("bright");
    pBoingy = g ("boingy"); pDrive = g ("drive");
    pEq1Gain = g ("eq1gain"); pEq1Freq = g ("eq1freq"); pEq1Width = g ("eq1width");
    pEq2Gain = g ("eq2gain"); pEq2Freq = g ("eq2freq"); pEq2Width = g ("eq2width");
    pDry = g ("dry"); pWet = g ("wet"); pWidth = g ("width"); pPan = g ("pan");
}

bool CoilProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
}

static void panGains (float pan, float& gl, float& gr)   // pan -1..1, centre = unity
{
    const float th = (pan + 1.0f) * (float) spr::kPi * 0.25f;
    gl = std::cos (th) * 1.41421356f; gr = std::sin (th) * 1.41421356f;
}

void CoilProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = jmax (1, samplesPerBlock);
    reverb.prepare (sampleRate);
    lastModel = -1;
    for (auto& b : eq1) b.reset();
    for (auto& b : eq2) b.reset();
    for (auto* s : { &dryS, &wetS, &widthS, &panLS, &panRS }) s->reset (sampleRate, 0.03);
    float gl, gr; panGains (*pPan * 0.01f, gl, gr);
    dryS.setCurrentAndTargetValue (*pDry * 0.01f);
    wetS.setCurrentAndTargetValue (*pWet * 0.015f);
    widthS.setCurrentAndTargetValue (*pWidth * 0.01f);
    panLS.setCurrentAndTargetValue (gl); panRS.setCurrentAndTargetValue (gr);
}

void CoilProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    for (int pos = 0; pos < total; pos += maxBlock)     // hosts shouldn't exceed maxBlock, but be safe
    {
        AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, jmin (maxBlock, total - pos));
        processChunk (chunk);
    }
}

void CoilProcessor::processChunk (AudioBuffer<float>& buffer)
{
    const int numCh = jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (n == 0 || numCh == 0) return;

    const int model = (int) *pModel;
    if (model != lastModel) { reverb.setModel (model); reverb.reset(); lastModel = model; }
    reverb.setParams (*pDecay, *pTension, *pBright, *pBoingy, *pDrive);

    // two-band wet EQ: at the lowest width a band becomes a shelf (low for band 1, high for band 2)
    const float g1 = *pEq1Gain, g2 = *pEq2Gain;
    const bool useEq1 = std::abs (g1) > 0.05f, useEq2 = std::abs (g2) > 0.05f;
    for (int c = 0; c < 2; ++c)
    {
        if (useEq1) { const float w = *pEq1Width; if (w < 0.04f) eq1[(size_t) c].lowShelf (fs, *pEq1Freq, g1);
                      else eq1[(size_t) c].peaking (fs, *pEq1Freq, g1, 4.0 * std::pow (0.125, (double) w)); }
        if (useEq2) { const float w = *pEq2Width; if (w < 0.04f) eq2[(size_t) c].highShelf (fs, *pEq2Freq, g2);
                      else eq2[(size_t) c].peaking (fs, *pEq2Freq, g2, 4.0 * std::pow (0.125, (double) w)); }
    }

    float gl, gr; panGains (*pPan * 0.01f, gl, gr);
    dryS.setTargetValue (*pDry * 0.01f);
    wetS.setTargetValue (*pWet * 0.015f);
    widthS.setTargetValue (*pWidth * 0.01f);
    panLS.setTargetValue (gl); panRS.setTargetValue (gr);

    float* chL = buffer.getWritePointer (0);
    float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float inL = chL[i], inR = chR != nullptr ? chR[i] : inL;
        float wl, wr; reverb.process (inL, inR, wl, wr);

        double l = wl, r = wr;
        if (useEq1) { l = eq1[0].process (l); r = eq1[1].process (r); }
        if (useEq2) { l = eq2[0].process (l); r = eq2[1].process (r); }

        const float dry = dryS.getNextValue(), wet = wetS.getNextValue(), wid = widthS.getNextValue();
        const float pl = panLS.getNextValue(), pr = panRS.getNextValue();
        const double mid = 0.5 * (l + r), side = 0.5 * (l - r) * wid;
        const float wetL = (float) (mid + side) * pl * wet, wetR = (float) (mid - side) * pr * wet;

        if (chR != nullptr) { chL[i] = inL * dry + wetL; chR[i] = inR * dry + wetR; }
        else                  chL[i] = inL * dry + 0.5f * (wetL + wetR);
    }
}

AudioProcessorEditor* CoilProcessor::createEditor() { return new CoilEditor (*this); }

void CoilProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void CoilProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CoilProcessor(); }
