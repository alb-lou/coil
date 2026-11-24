#include "PluginEditor.h"

using namespace juce;

namespace
{
    enum Fmt { Pct01, Seconds, Hz, Gain, Percent, Pan };
    struct KnobDef { const char* id; const char* name; Fmt fmt; };
    // row 1: spring | row 2: EQ | row 3: mix matrix
    const KnobDef kDefs[15] = {
        { "decay", "Decay", Seconds }, { "tension", "Tension", Pct01 }, { "bright", "Bright", Pct01 },
        { "boingy", "Boingy", Pct01 }, { "drive", "Drive", Pct01 },
        { "eq1gain", "Low Gain", Gain }, { "eq1freq", "Low Freq", Hz }, { "eq1width", "Low Width", Pct01 },
        { "eq2gain", "High Gain", Gain }, { "eq2freq", "High Freq", Hz }, { "eq2width", "High Width", Pct01 },
        { "dry", "Dry", Percent }, { "wet", "Wet", Percent }, { "width", "Wet Width", Percent }, { "pan", "Wet Pan", Pan } };
    const char* kModelNames[4] = { "Vintage", "Keys", "Hi-Gain", "Studio" };
    const Colour accent (0xff3fb6a8);
}

CoilEditor::CoilEditor (CoilProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (640, 520);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i]; const auto& d = kDefs[i];
        k.slider.setColour (Slider::rotarySliderFillColourId, accent);
        k.slider.setColour (Slider::thumbColourId, Colours::white);
        k.slider.setColour (Slider::textBoxTextColourId, Colours::white);
        k.slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        k.slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 20);
        switch (d.fmt)
        {
            case Pct01:   k.slider.textFromValueFunction = [] (double v) { return String ((int) std::round (v * 100)); };
                          k.slider.valueFromTextFunction = [] (const String& t) { return t.getDoubleValue() / 100.0; }; break;
            case Seconds: k.slider.setTextValueSuffix (" s"); k.slider.setNumDecimalPlacesToDisplay (2); break;
            case Hz:      k.slider.textFromValueFunction = [] (double v) { return v >= 1000 ? String (v / 1000.0, 2) + " kHz" : String ((int) v) + " Hz"; }; break;
            case Gain:    k.slider.setTextValueSuffix (" dB"); k.slider.setNumDecimalPlacesToDisplay (1); break;
            case Percent: k.slider.setTextValueSuffix (" %"); k.slider.setNumDecimalPlacesToDisplay (0); break;
            case Pan:     k.slider.textFromValueFunction = [] (double v) { return std::abs (v) < 0.5 ? String ("C") : (v < 0 ? "L " : "R ") + String ((int) std::abs (v)); }; break;
        }
        k.slider.setDoubleClickReturnValue (true, k.slider.getValue());
        addAndMakeVisible (k.slider);
        k.label.setText (d.name, dontSendNotification);
        k.label.setJustificationType (Justification::centred);
        k.label.setColour (Label::textColourId, Colours::lightgrey);
        addAndMakeVisible (k.label);
        k.att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, k.slider);
        if (auto* prm = proc.apvts.getParameter (d.id)) k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }

    for (size_t i = 0; i < modelButtons.size(); ++i)
    {
        auto& b = modelButtons[i];
        b.setButtonText (kModelNames[i]);
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1);
        b.setColour (TextButton::buttonOnColourId, accent);
        b.onClick = [this, i]
        {
            if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("model")))
            { c->beginChangeGesture(); *c = (int) i; c->endChangeGesture(); }
        };
        addAndMakeVisible (b);
    }
    startTimerHz (15);
    timerCallback();
}

CoilEditor::~CoilEditor() { stopTimer(); }

void CoilEditor::timerCallback()
{
    const int m = (int) *proc.apvts.getRawParameterValue ("model");
    for (size_t i = 0; i < modelButtons.size(); ++i)
        modelButtons[i].setToggleState ((int) i == m, dontSendNotification);
}

void CoilEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff1b1d20));
    g.setColour (accent);
    g.setFont (FontOptions (26.0f, Font::bold));
    g.drawText ("COIL", 20, 10, 200, 34, Justification::centredLeft);
    g.setColour (Colours::grey);
    g.setFont (FontOptions (13.0f));
    g.drawText ("spring reverb", 20, 38, 200, 18, Justification::centredLeft);
    g.setFont (FontOptions (12.0f));
    g.drawText ("SPRING", 20, 108, 100, 14, Justification::centredLeft);
    g.drawText ("WET EQ", 20, 270, 100, 14, Justification::centredLeft);
    g.drawText ("MIX MATRIX", 20, 392, 120, 14, Justification::centredLeft);
}

void CoilEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (50);
    auto modelRow = area.removeFromTop (34);
    const int mw = modelRow.getWidth() / (int) modelButtons.size();
    for (auto& b : modelButtons) b.setBounds (modelRow.removeFromLeft (mw).reduced (4, 0));

    const int rowCounts[3] = { 5, 6, 4 };
    int idx = 0;
    for (int r = 0; r < 3; ++r)
    {
        area.removeFromTop (r == 0 ? 22 : 16);
        auto row = area.removeFromTop (r == 0 ? 120 : 112);
        const int kw = row.getWidth() / rowCounts[r];
        for (int c = 0; c < rowCounts[r]; ++c, ++idx)
        {
            auto cell = row.removeFromLeft (kw);
            knobs[(size_t) idx].label.setBounds (cell.removeFromTop (18));
            knobs[(size_t) idx].slider.setBounds (cell);
        }
    }
}
