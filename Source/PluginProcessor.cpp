#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

static float raw (APVTS& a, const char* id) { return a.getRawParameterValue (id)->load(); }

kwlab::Params readParams (APVTS& a)
{
    kwlab::Params p;
    p.arch = (int) raw (a, "arch");
    p.levelMatch = raw (a, "levelMatch") > 0.5f;
    p.output = raw (a, "output");
    p.bxLow = raw (a, "bx_low");     p.bxHigh = raw (a, "bx_high");   p.bxMid = raw (a, "bx_mid");
    p.bxSweep = raw (a, "bx_sweep"); p.bxLean = raw (a, "bx_lean");   p.bxFeel = raw (a, "bx_feel");
    p.bxLowC = (int) raw (a, "bx_lowc"); p.bxHighC = (int) raw (a, "bx_highc"); p.bxRoute = (int) raw (a, "bx_route");
    p.bxVibe = (int) raw (a, "bx_vibe"); p.bxQ = (int) raw (a, "bx_q");
    p.bbBody = raw (a, "bbc_body");  p.bbBright = raw (a, "bbc_bright"); p.bbCut = raw (a, "bbc_cut");
    p.rtBass = raw (a, "rt_bass");   p.rtLowMid = raw (a, "rt_lowmid");  p.rtHighMid = raw (a, "rt_highmid"); p.rtTreble = raw (a, "rt_treble");
    p.ssPunch = raw (a, "ss_punch"); p.ssEdge = raw (a, "ss_edge");      p.ssDepth = raw (a, "ss_depth");      p.ssPresence = raw (a, "ss_presence");
    return p;
}

APVTS::ParameterLayout LabProcessor::createLayout()
{
    using F = juce::AudioParameterFloat; using C = juce::AudioParameterChoice; using B = juce::AudioParameterBool;
    using R = juce::NormalisableRange<float>; using ID = juce::ParameterID; using SA = juce::StringArray;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto bip  = [&] (const char* id, const char* name) { p.push_back (std::make_unique<F> (ID { id, 1 }, name, R (-1.f, 1.f, 0.001f), 0.f)); };
    auto uni  = [&] (const char* id, const char* name, float def = 0.f) { p.push_back (std::make_unique<F> (ID { id, 1 }, name, R (0.f, 1.f, 0.001f), def)); };
    auto choice = [&] (const char* id, const char* name, SA items, int def) { p.push_back (std::make_unique<C> (ID { id, 1 }, name, items, def)); };

    choice ("arch", "EQ Architecture", SA { "BAXENDALL", "BBC EQ", "ROTARD", "SUPERSCHALL" }, 0);
    p.push_back (std::make_unique<B> (ID { "bypass", 1 }, "Bypass", false));
    p.push_back (std::make_unique<B> (ID { "levelMatch", 1 }, "Level Match", false));
    p.push_back (std::make_unique<F> (ID { "output", 1 }, "Output (dB)", R (-12.f, 12.f, 0.1f), 0.f));
    // BAXENDALL
    bip ("bx_low", "Low Bax"); bip ("bx_high", "High Bax"); bip ("bx_mid", "Mid");
    uni ("bx_sweep", "Mid Sweep", 0.5f); bip ("bx_lean", "Lean"); uni ("bx_feel", "Feel", 0.5f);
    choice ("bx_lowc",  "Low Contour",  SA { "Loose", "Relaxed", "Flat", "Firm", "Tight" }, 2);
    choice ("bx_highc", "High Contour", SA { "Smooth", "Flat", "Air", "Sparkle", "Edge" }, 1);
    choice ("bx_route", "Mid Routing",  SA { "Pre-Bax", "Post-Bax", "Parallel" }, 1);
    choice ("bx_vibe",  "Vibe",         SA { "Dark", "Bright" }, 1);
    choice ("bx_q",     "Mid Q",        SA { "Broad", "Narrow" }, 0);
    // BBC EQ
    bip ("bbc_body", "Body"); bip ("bbc_bright", "Bright"); uni ("bbc_cut", "Cut");
    // ROTARD
    uni ("rt_bass", "Bass R"); uni ("rt_lowmid", "Low Mid R"); uni ("rt_highmid", "High Mid R"); uni ("rt_treble", "Treble R");
    // SUPERSCHALL
    uni ("ss_punch", "Low Punch"); uni ("ss_edge", "High Edge"); bip ("ss_depth", "Depth"); bip ("ss_presence", "Presence");
    return { p.begin(), p.end() };
}

LabProcessor::LabProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KW_EQ_LAB", createLayout())
{
}

bool LabProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void LabProcessor::prepareToPlay (double rate, int)
{
    sr = rate;
    for (auto& c : chans) c.reset();
    cur = readParams (apvts);
    lastArch = cur.arch;
}

void LabProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (raw (apvts, "bypass") > 0.5f) return;            // settings are preserved while bypassed

    const kwlab::Params target = readParams (apvts);
    if (target.arch != lastArch) { for (auto& c : chans) c.reset(); cur = target; lastArch = target.arch; }

    const int n = buf.getNumSamples(), nch = juce::jmin (buf.getNumChannels(), 2);
    const int step = 32;
    const double alpha = 1.0 - std::exp (-(double) step / (sr * 0.02));      // ~20 ms smoothing
    for (int start = 0; start < n; start += step)
    {
        const int len = juce::jmin (step, n - start);
        kwlab::smoothToward (cur, target, alpha);
        const kwlab::Model model = kwlab::makeModel (cur, sr);
        for (int ch = 0; ch < nch; ++ch)
        {
            float* d = buf.getWritePointer (ch) + start;
            for (int i = 0; i < len; ++i) d[i] = kwlab::processSample (model, chans[ch], d[i]);
        }
    }
}

void LabProcessor::selectSlot (int s)
{
    if (s == slot || s < 0 || s > 1) return;
    slotState[slot] = apvts.copyState();
    slot = s;
    if (slotState[slot].isValid()) apvts.replaceState (slotState[slot]);   // first visit to B keeps the current settings
}
void LabProcessor::copyCurrentToOtherSlot() { slotState[1 - slot] = apvts.copyState(); }

juce::AudioProcessorEditor* LabProcessor::createEditor() { return new LabEditor (*this); }

void LabProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void LabProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LabProcessor(); }
