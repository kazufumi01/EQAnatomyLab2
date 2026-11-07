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
                        private juce::Timer
{
public:
    ZoomButton zoom;
    explicit EQAnatomyView (EQAnatomyProcessor&);
    ~EQAnatomyView() override;

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
    juce::TextButton resBtn, learnBtn, langBtn, resetBtn, undoBtn, redoBtn, earBtn, morphTo;
    juce::TextButton slotBtn[4];
    ResetCombo curveBox;
    juce::Slider morph;
    void setEar (bool on);
    void applySkin (int i);
    void applySurface (int i);
    void applyScale (int i);
    // body
    GraphView graph { ph };
    SatStrip satStrip { ph };
    BandPopup detail { ph, true };     // the selected band's detail panel (replaces the band cards)
    IRView irView { ph };
    OutputDrag outDrag { ph };
    SoloView viewSolo { ph };
    juce::TextButton autoBtn, deltaBtn, bypassBtn, soloBtn;
    LessonCard lesson { ph };
    IOMeter io { ph };
    EarCard earCard { ph };

    // analyzer
    juce::dsp::FFT fft { 13 };
    juce::dsp::WindowingFunction<float> window { 8192, juce::dsp::WindowingFunction<float>::hann };
    // four rings (Mid, Side, Transient, Body), written in step; only the layers on screen are transformed
    std::array<std::vector<float>, 4> rings { std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f),
                                              std::vector<float> (8192, 0.0f), std::vector<float> (8192, 0.0f) };
    int ringPos = 0;
    std::array<std::vector<float>, 4> bins { std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f),
                                             std::vector<float> (16384, 0.0f), std::vector<float> (16384, 0.0f) };
    void layerSpectrum (int src, std::vector<float>& out);     // src: 0 Stereo, 1 Mid, 2 Side, 3 Trans, 4 Body
    std::vector<float> spec = std::vector<float> (401, -140.0f), specB = std::vector<float> (401, -140.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQAnatomyView)
};

class EQAnatomyEditor : public ScalingEditor<EQAnatomyView, EQAnatomyProcessor>
{
public:
    explicit EQAnatomyEditor (EQAnatomyProcessor& p) : ScalingEditor (p, 1160, 760, "eqanatomy.zoom2") {}
};
