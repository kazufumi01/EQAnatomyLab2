#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "EQCore.h"

// =============================================================================
//  Parameter IDs and layout
// =============================================================================
namespace pid
{
inline juce::String band (int i, const char* what) { return "b" + juce::String (i + 1) + "_" + what; }
static const char* mode = "mode";
static const char* res = "res";
static const char* qmode = "qmode";
static const char* output = "output";
static const char* autoGain = "autogain";
static const char* delta = "delta";
static const char* bypass = "bypass";
static const char* preSolo = "presolo";
static const char* satOn = "sat_on";
static const char* satType = "sat_type";
static const char* satLo = "sat_lo";
static const char* satHi = "sat_hi";
static const char* satDrive = "sat_drive";
static const char* satMix = "sat_mix";
static const char* satDelta = "sat_delta";
static const char* satAuto = "sat_auto";
static const char* satOS = "sat_os";
static const char* tbSens = "tb_sens";      // transient detector: dB added to the attack-over-body measure (0 = as before)
static const char* tbWidth = "tb_width";    // transient detector: dB over which the mask goes 0 → 1 (6 = as before)
static const char* tbAtt = "tb_att";        // transient detector: how fast the mask rises, ms (2 = as before)
static const char* tbMethod = "tb_method";  // transient detector: 0 Ratio (fast / slow envelope, as before), 1 Slope (rise of the envelope)
static const char* tbLc = "tb_lc";          // detector-only EQ (never in the output): low cut, Hz (20 = off)
static const char* tbE1f = "tb_e1f"; static const char* tbE1g = "tb_e1g"; static const char* tbE1q = "tb_e1q";
static const char* tbE2f = "tb_e2f"; static const char* tbE2g = "tb_e2g"; static const char* tbE2q = "tb_e2q";
}

inline const juce::StringArray& typeNames()
{
    static const juce::StringArray t { "Bell", "Low shelf", "High shelf", "Low cut", "High cut", "Notch", "All-pass", "Tilt shelf", "Flat tilt" };
    return t;
}
inline const juce::StringArray& slopeNames()
{
    static const juce::StringArray s { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    return s;
}

// Flat start: six bands spread across the spectrum, all off
struct BandDefault { eqcore::Type type; double f; double q; };
inline const BandDefault& bandDefault (int i)
{
    static const BandDefault d[] = { { eqcore::Type::LowCut, 30, 0.707 }, { eqcore::Type::LowShelf, 100, 0.707 },
                                     { eqcore::Type::Bell, 400, 1.0 },   { eqcore::Type::Bell, 1500, 1.0 },
                                     { eqcore::Type::HighShelf, 6000, 0.707 }, { eqcore::Type::HighCut, 18000, 0.707 },
                                     { eqcore::Type::Bell, 250, 1.0 },   { eqcore::Type::Bell, 4000, 1.0 } };   // 7, 8 (0.1.1)
    return d[juce::jlimit (0, eqcore::kBands - 1, i)];
}

inline void addBandMain (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i, int ver)
{
    using namespace juce;
    auto freqRange = NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 1.0f);
    freqRange.setSkewForCentre (632.0f);
    auto qRange = NormalisableRange<float> (0.1f, 18.0f, 0.0f, 1.0f);
    qRange.setSkewForCentre (1.0f);
        const auto& d = bandDefault (i);
        const String n = "Band " + String (i + 1) + " ";
        l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::band (i, "on"), ver }, n + "On", false));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::band (i, "type"), ver }, n + "Type", typeNames(), (int) d.type));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "freq"), ver }, n + "Freq", freqRange, (float) d.f,
                                                      AudioParameterFloatAttributes().withLabel ("Hz")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "gain"), ver }, n + "Gain",
                                                      NormalisableRange<float> (-30.0f, 30.0f, 0.01f), 0.0f,
                                                      AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "q"), ver }, n + "Q", qRange, (float) d.q));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::band (i, "slope"), ver }, n + "Slope", slopeNames(), 3));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::band (i, "dyn"), ver }, n + "Dynamic", false));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "thr"), ver }, n + "Threshold",
                                                      NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -24.0f, AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "range"), ver }, n + "Range",
                                                      NormalisableRange<float> (-18.0f, 18.0f, 0.1f), -6.0f, AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "below"), ver }, n + "Below range",
                                                      NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::band (i, "src"), ver }, n + "Detector", StringArray { "Input", "Sidechain", "Mid", "Side" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::band (i, "phase"), ver }, n + "Phase", StringArray { "Min", "Lin" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::band (i, "ch"), ver }, n + "Channel", StringArray { "Stereo", "Mid", "Side", "Left", "Right" }, 0));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "tb"), ver }, n + "Transient/Body",
                                                      NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f));
    }
inline void addBandDyn (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i, int ver)
{
    using namespace juce;
        const String n = "Band " + String (i + 1) + " ";
        auto attRange = NormalisableRange<float> (0.5f, 200.0f, 0.0f, 1.0f);   attRange.setSkewForCentre (10.0f);
        auto relRange = NormalisableRange<float> (5.0f, 2000.0f, 0.0f, 1.0f);  relRange.setSkewForCentre (100.0f);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "att"), ver }, n + "Dyn attack", attRange, 5.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "rel"), ver }, n + "Dyn release", relRange, 120.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::band (i, "knee"), ver }, n + "Dyn knee", NormalisableRange<float> (0.0f, 24.0f, 0.1f), 12.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto freqRange = NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 1.0f);
    freqRange.setSkewForCentre (632.0f);
    auto qRange = NormalisableRange<float> (0.1f, 18.0f, 0.0f, 1.0f);
    qRange.setSkewForCentre (1.0f);
    for (int i = 0; i < 6; ++i) addBandMain (l, i, 1);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::mode, 1 }, "Mode", StringArray { "Zero latency", "Analog", "Linear" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::res, 1 }, "Linear resolution", StringArray { "Low", "Mid", "High" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::qmode, 1 }, "Curve", StringArray { "Clean Const", "Clean Prop", "British A", "US 500", "Console E", "Tube Passive", "Precision" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::output, 1 }, "Output", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::autoGain, 1 }, "Auto gain", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::delta, 1 }, "Delta", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::bypass, 1 }, "Bypass", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::preSolo, 1 }, "Pre-ring solo", false));
    // band saturator
    auto bandRange = NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 1.0f);
    bandRange.setSkewForCentre (632.0f);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::satOn, 1 }, "Sat on", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::satType, 1 }, "Sat type", StringArray { "Tape", "Tube", "Clip", "Transformer", "Class A", "Exciter" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::satLo, 1 }, "Sat low edge", bandRange, 200.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::satHi, 1 }, "Sat high edge", bandRange, 5000.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::satDrive, 1 }, "Sat drive", NormalisableRange<float> (0.0f, 36.0f, 0.1f), 9.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::satMix, 1 }, "Sat mix", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::satDelta, 1 }, "Sat delta", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pid::satAuto, 1 }, "Sat auto level", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::satOS, 1 }, "Sat oversampling", StringArray { "1x", "2x", "4x" }, 0));

    // ---- added later: always appended, and every default equals the old fixed behaviour ----
    for (int i = 0; i < 6; ++i) addBandDyn (l, i, 2);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tbSens, 2 }, "Transient sensitivity", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tbWidth, 2 }, "Transient width", NormalisableRange<float> (1.0f, 18.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tbAtt, 2 }, "Transient attack", NormalisableRange<float> (0.1f, 2.0f, 0.01f), 2.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::tbMethod, 2 }, "Transient method", StringArray { "Ratio", "Slope" }, 1));   // new instances: Slope. Sessions saved before 0.1.1 are pinned to Ratio in setStateInformation
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::tbLc, 2 }, "Detector low cut", NormalisableRange<float> (20.0f, 1000.0f, 0.0f, 0.4f), 20.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
    for (int k = 1; k <= 2; ++k)
    {
        const String n (k);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { k == 1 ? pid::tbE1f : pid::tbE2f, 2 }, "Detector EQ " + n + " freq", NormalisableRange<float> (40.0f, 16000.0f, 0.0f, 0.25f), k == 1 ? 200.0f : 3000.0f, AudioParameterFloatAttributes().withLabel ("Hz")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { k == 1 ? pid::tbE1g : pid::tbE2g, 2 }, "Detector EQ " + n + " gain", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { k == 1 ? pid::tbE1q : pid::tbE2q, 2 }, "Detector EQ " + n + " Q", NormalisableRange<float> (0.3f, 8.0f, 0.01f, 0.5f), 1.0f));
    }
    // ---- 0.1.1: bands 7 and 8, appended (off by default) ----
    for (int i = 6; i < eqcore::kBands; ++i) { addBandMain (l, i, 3); addBandDyn (l, i, 3); }
    return l;
}
