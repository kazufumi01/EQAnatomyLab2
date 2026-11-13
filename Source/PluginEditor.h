#pragma once
#include "PluginProcessor.h"
#include "ScalingEditor.h"
#include "Components.h"

// =============================================================================
//  v2 layout (1160 × 760)
//    header 44 · big curve + IN/OUT meters · saturator strip ·
//    bottom row: bands | impulse | buttons | lesson (or ear training)
// =============================================================================
class EQAnatomyView : public juce::Component,
                        private juce::Timer,
                        private juce::AudioProcessorParameter::Listener
{
public:
    ZoomButton zoom;
    explicit EQAnatomyView (EQAnatomyProcessor&);
    ~EQAnatomyView() override;
    // any control that starts a change gesture becomes the target of the arrow keys
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshTexts();
    void pullAnalyzer();
    void updateHover();

    EQAnatomyProcessor& proc;
    theme::LookAndFeel lnf;
    ParamHelp ph { proc };

    // header
    ResetCombo modeBox;            // Zero latency / Analog / Linear in one menu
    LongPressButton learnBtn;
    juce::TextButton helpBtn, resBtn, langBtn, resetBtn, undoBtn, redoBtn, earBtn, morphTo;
    juce::TextButton slotBtn[4];
    ResetCombo curveBox;
    juce::Slider morph;
    void setEar (bool on);
    bool lowerOpen() const; void setLower (bool on);
    void userPresetAction (int r);
    juce::Rectangle<int> latRect, presetGroup;
    void applySkin (int i);
    void applySurface (int i);
    void applyScale (int i);
    // body
    GraphView graph { ph };
    SatStrip satStrip { ph };
    ResStrip resStrip { ph };
    ModuleTab satTab { ph, pid::satOn, 0, "SAT", theme::sat }, resTab { ph, pid::rsOn, 1, "RES", theme::res };   // SAT | RES share the lower-left slot
    IRView irView { ph };
    OutputDrag outDrag { ph };
    SoloView viewSolo { ph };
    juce::TextButton deltaBtn, bypassBtn;
    LessonCard lesson { ph };
    IOMeter io { ph };
    EarCard earCard { ph };

    // analyzer
    juce::dsp::FFT fft { 13 };
    juce::dsp::WindowingFunction<float> window { 8192, juce::dsp::WindowingFunction<float>::hann };
    // four rings (Mid, Side, Transient, Body), written in step; only the layers on screen are transformed
    // + input Mid, input Side, sidechain (0.1.1)
    std::array<std::vector<float>, 7> rings { std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f),
                                              std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f),
                                              std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f) };
    int ringPos = 0;
    std::shared_ptr<LinkTap> linkTap;        // the instance shown by analyzer B (link), found by name
    std::array<std::vector<float>, 7> bins { std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f),
                                             std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f),
                                             std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f) };
    void layerSpectrum (int src, std::vector<float>& out);     // src: 0 Stereo, 1 Mid, 2 Side, 3 Trans, 4 Body, 5 Input (before the EQ), 6 Sidechain, 7 another instance (link)
    std::vector<float> spec = std::vector<float> (401, -140.0f), specB = std::vector<float> (401, -140.0f);
    // stereo field per band (1/6 octave): smoothed Mid / Side powers and their cross term, [0] input, [1] output
    void fieldUpdate();
    std::array<std::vector<double>, 2> fPM, fPS, fX, fY;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAnatomyView)
};

class EQAnatomyEditor : public ScalingEditor<EQAnatomyView, EQAnatomyProcessor>
{
public:
    explicit EQAnatomyEditor (EQAnatomyProcessor& p) : ScalingEditor (p, 1160, 760, "eqa2v1.zoom2") {}
};
