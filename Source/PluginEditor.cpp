#include "PluginEditor.h"

using namespace juce;

Colour LabCurve::groupColour (int g)
{
    static const uint32 c[] = { 0xfff2b84b, 0xff5fd38d, 0xffff7a59, 0xff5aa9ff };
    return Colour (c[g]);
}

LabCurve::LabCurve (LabProcessor& p) : proc (p) { startTimerHz (30); }

void LabCurve::paint (Graphics& g)
{
    g.fillAll (Colour (0xff12151c));
    auto plot = getLocalBounds().toFloat().withTrimmedLeft (44).withTrimmedBottom (24).withTrimmedTop (10).withTrimmedRight (12);
    const double fMin = 20, fMax = 20000, dbMax = 15.0;
    auto xOf = [&] (double f) { return plot.getX() + (float) (std::log (f / fMin) / std::log (fMax / fMin)) * plot.getWidth(); };
    auto yOf = [&] (double d) { return plot.getCentreY() - (float) (d / dbMax) * (plot.getHeight() * 0.5f); };

    g.setFont (11.0f);
    for (int d = -15; d <= 15; d += 5)
    {
        g.setColour (d == 0 ? Colour (0xff4a5263) : Colour (0xff262c38));
        g.drawHorizontalLine ((int) yOf (d), plot.getX(), plot.getRight());
        g.setColour (Colour (0xff8a93a6));
        g.drawText (String (d) + " dB", Rectangle<float> (0, yOf (d) - 8, 40, 16).toNearestInt(), Justification::centredRight);
    }
    const double marks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* names[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
    for (int i = 0; i < 10; ++i)
    {
        g.setColour (Colour (0xff262c38)); g.drawVerticalLine ((int) xOf (marks[i]), plot.getY(), plot.getBottom());
        g.setColour (Colour (0xff8a93a6)); g.drawText (names[i], Rectangle<float> (xOf (marks[i]) - 20, plot.getBottom() + 3, 40, 16).toNearestInt(), Justification::centred);
    }

    const double sr = 48000.0;
    const auto params = readParams (proc.apvts);
    const auto model = kwlab::makeModel (params, sr);

    auto curveFor = [&] (std::function<double (double)> fn, double* maxAbs = nullptr)
    {
        Path p; bool first = true;
        for (int i = 0; i <= 400; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, i / 400.0), d = fn (f);
            if (maxAbs) *maxAbs = std::max (*maxAbs, std::abs (d));
            const float y = jlimit (plot.getY(), plot.getBottom(), yOf (d));
            if (first) { p.startNewSubPath (xOf (f), y); first = false; } else p.lineTo (xOf (f), y);
        }
        return p;
    };

    Path total = curveFor ([&] (double f) { return kwlab::totalDb (model, f, sr); });
    Path fill (total); fill.lineTo (plot.getRight(), yOf (0)); fill.lineTo (plot.getX(), yOf (0)); fill.closeSubPath();
    g.setColour (Colours::white.withAlpha (0.08f)); g.fillPath (fill);

    const int ng = kwlab::numGroups (params.arch);
    for (int gi = 0; gi < ng; ++gi)
    {
        double mx = 0;
        Path pth = curveFor ([&] (double f) { return kwlab::groupDb (model, gi, f, sr); }, &mx);
        if (mx < 0.05) continue;
        g.setColour (groupColour (gi).withAlpha (0.9f));
        g.strokePath (pth, PathStrokeType (1.5f));
    }
    g.setColour (Colours::white); g.strokePath (total, PathStrokeType (2.6f, PathStrokeType::curved));

    // legend (top right)
    g.setFont (11.0f);
    float lx = plot.getRight() - 6;
    for (int gi = ng - 1; gi >= 0; --gi)
    {
        const String t (kwlab::groupName (params.arch, gi));
        const float w = g.getCurrentFont().getStringWidthFloat (t) + 22;
        lx -= w;
        g.setColour (groupColour (gi)); g.fillRoundedRectangle (lx, plot.getY() + 8, 12, 4, 2);
        g.setColour (Colour (0xffc9ceda)); g.drawText (t, Rectangle<float> (lx + 15, plot.getY() + 2, w - 15, 16).toNearestInt(), Justification::centredLeft);
    }
    g.setColour (Colours::white); g.fillRoundedRectangle (lx - 52, plot.getY() + 8, 12, 4, 2);
    g.setColour (Colour (0xffc9ceda)); g.drawText ("Total", Rectangle<float> (lx - 36, plot.getY() + 2, 40, 16).toNearestInt(), Justification::centredLeft);
    if (params.levelMatch)
    {
        g.setColour (Colour (0xff8a93a6));
        g.drawText ("level match: " + String (model.levelDb, 1) + " dB", Rectangle<float> (plot.getX() + 8, plot.getY() + 2, 200, 16).toNearestInt(), Justification::centredLeft);
    }
}

// ------------------------------------------------------------------------------------------
LabEditor::LabEditor (LabProcessor& p) : AudioProcessorEditor (&p), proc (p), curve (p)
{
    setSize (1080, 700);
    addAndMakeVisible (curve);

    archLabel.setText ("EQ", dontSendNotification); archLabel.setColour (Label::textColourId, Colour (0xff8a93a6));
    archBox.addItemList ({ "BAXENDALL", "BBC EQ", "ROTARD", "SUPERSCHALL" }, 1);
    addAndMakeVisible (archLabel); addAndMakeVisible (archBox);
    archAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "arch", archBox);

    for (auto* b : { &bypassBtn, &levelBtn }) { b->setClickingTogglesState (true); addAndMakeVisible (b); }
    bypassBtn.setColour (TextButton::buttonOnColourId, Colour (0xffd9534f));
    levelBtn.setColour (TextButton::buttonOnColourId, Colour (0xff5fd38d)); levelBtn.setColour (TextButton::textColourOnId, Colour (0xff10131a));
    bypassAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);
    levelAtt  = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "levelMatch", levelBtn);

    for (auto* b : { &aBtn, &bBtn }) { b->setClickingTogglesState (true); b->setRadioGroupId (7); addAndMakeVisible (b); b->setColour (TextButton::buttonOnColourId, Colour (0xfff2b84b)); b->setColour (TextButton::textColourOnId, Colour (0xff10131a)); }
    aBtn.setToggleState (proc.getSlot() == 0, dontSendNotification); bBtn.setToggleState (proc.getSlot() == 1, dontSendNotification);
    aBtn.onClick = [this] { if (aBtn.getToggleState()) proc.selectSlot (0); };
    bBtn.onClick = [this] { if (bBtn.getToggleState()) proc.selectSlot (1); };
    addAndMakeVisible (copyBtn);
    copyBtn.onClick = [this] { proc.copyCurrentToOtherSlot(); };

    const auto c0 = LabCurve::groupColour (0), c1 = LabCurve::groupColour (1), c2 = LabCurve::groupColour (2), c3 = LabCurve::groupColour (3);
    // BAXENDALL
    addKnob (0, "bx_low",   "LOW BAX",      c0);
    addKnob (0, "bx_high",  "HIGH BAX",     c0);
    addKnob (0, "bx_mid",   "MID",          c1);
    addKnob (0, "bx_sweep", "MID SWEEP",    c1);
    addKnob (0, "bx_lean",  "LEAN",         c2);
    addKnob (0, "bx_feel",  "FEEL",         c2);
    addKnob (0, "bx_lowc",  "LOW CONTOUR",  c3, { "Loose", "Relaxed", "Flat", "Firm", "Tight" });
    addKnob (0, "bx_highc", "HIGH CONTOUR", c3, { "Smooth", "Flat", "Air", "Sparkle", "Edge" });
    addKnob (0, "bx_route", "MID ROUTING",  c1, { "Pre-Bax", "Post-Bax", "Parallel" });
    addCombo (0, "bx_vibe", "VIBE",  { "Dark", "Bright" });
    addCombo (0, "bx_q",    "MID Q", { "Broad", "Narrow" });
    // BBC EQ
    addKnob (1, "bbc_body",   "BODY",   c0);
    addKnob (1, "bbc_bright", "BRIGHT", c1);
    addKnob (1, "bbc_cut",    "CUT",    c2);
    // ROTARD
    addKnob (2, "rt_bass",    "BASS R",     c0);
    addKnob (2, "rt_lowmid",  "LOW MID R",  c1);
    addKnob (2, "rt_highmid", "HIGH MID R", c2);
    addKnob (2, "rt_treble",  "TREBLE R",   c3);
    // SUPERSCHALL
    addKnob (3, "ss_punch",    "LOW PUNCH", c0);
    addKnob (3, "ss_edge",     "HIGH EDGE", c0);
    addKnob (3, "ss_depth",    "DEPTH",     c1);
    addKnob (3, "ss_presence", "PRESENCE",  c2);
    // shared
    addKnob (-1, "output", "OUTPUT", Colour (0xffc9ceda), {}, " dB");

    applyVisibility();
    startTimerHz (10);
}

void LabEditor::addKnob (int arch, const String& id, const String& text, Colour c, const StringArray& steps, const String& suffix)
{
    auto k = std::make_unique<Knob>();
    k->arch = arch;
    k->slider.setSliderStyle (Slider::RotaryVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    k->slider.setColour (Slider::rotarySliderFillColourId, c);
    k->slider.setColour (Slider::thumbColourId, c);
    k->slider.setColour (Slider::textBoxTextColourId, Colour (0xffe8eaed));
    k->slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    if (suffix.isNotEmpty()) k->slider.setTextValueSuffix (suffix);
    k->label.setText (text, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setColour (Label::textColourId, c);
    k->label.setFont (Font (12.5f, Font::bold));
    addAndMakeVisible (k->slider); addAndMakeVisible (k->label);
    k->att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    if (steps.size() > 0)
    {
        auto* s = &k->slider;
        s->textFromValueFunction = [steps] (double v) { return steps[jlimit (0, steps.size() - 1, (int) std::round (v))]; };
        s->updateText();
    }
    knobs.push_back (std::move (k));
}

void LabEditor::addCombo (int arch, const String& id, const String& text, const StringArray& items)
{
    auto c = std::make_unique<Combo>();
    c->arch = arch;
    c->box.addItemList (items, 1);
    c->label.setText (text, dontSendNotification);
    c->label.setJustificationType (Justification::centredRight);
    c->label.setColour (Label::textColourId, Colour (0xffc9ceda));
    addAndMakeVisible (c->box); addAndMakeVisible (c->label);
    c->att = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c->box);
    combos.push_back (std::move (c));
}

void LabEditor::timerCallback()
{
    const int a = (int) proc.apvts.getRawParameterValue ("arch")->load();
    if (a != shownArch) applyVisibility();
    aBtn.setToggleState (proc.getSlot() == 0, dontSendNotification);
    bBtn.setToggleState (proc.getSlot() == 1, dontSendNotification);
}

void LabEditor::applyVisibility()
{
    shownArch = (int) proc.apvts.getRawParameterValue ("arch")->load();
    for (auto& k : knobs) { const bool v = (k->arch < 0 || k->arch == shownArch); k->slider.setVisible (v); k->label.setVisible (v); }
    for (auto& c : combos) { const bool v = (c->arch < 0 || c->arch == shownArch); c->box.setVisible (v); c->label.setVisible (v); }
    resized();
}

void LabEditor::paint (Graphics& g) { g.fillAll (Colour (0xff0f1115)); }

void LabEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (46).reduced (12, 8);
    archLabel.setBounds (top.removeFromLeft (28));
    archBox.setBounds (top.removeFromLeft (190));
    top.removeFromLeft (24);
    aBtn.setBounds (top.removeFromLeft (40)); top.removeFromLeft (4);
    bBtn.setBounds (top.removeFromLeft (40)); top.removeFromLeft (8);
    copyBtn.setBounds (top.removeFromLeft (120));
    bypassBtn.setBounds (top.removeFromRight (110)); top.removeFromRight (8);
    levelBtn.setBounds (top.removeFromRight (130));

    curve.setBounds (r.removeFromTop (310).reduced (8, 4));
    auto row = r.removeFromTop (190).reduced (8, 0);
    std::vector<Knob*> vis;
    for (auto& k : knobs) if (k->slider.isVisible()) vis.push_back (k.get());
    const int w = vis.empty() ? 100 : jmin (110, row.getWidth() / (int) vis.size());
    // shared OUTPUT knob stays at the far right; the rest are centred on the left
    for (auto* k : vis)
    {
        Rectangle<int> c;
        if (k->arch < 0) c = row.removeFromRight (w); else c = row.removeFromLeft (w);
        k->label.setBounds (c.removeFromTop (20));
        k->slider.setBounds (c.reduced (6, 0));
    }
    auto crow = r.removeFromTop (44).reduced (16, 6);
    for (auto& c : combos)
    {
        if (! c->box.isVisible()) continue;
        auto cell = crow.removeFromLeft (310);
        c->label.setBounds (cell.removeFromLeft (70));
        c->box.setBounds (cell.reduced (8, 0).withWidth (170));
    }
}
