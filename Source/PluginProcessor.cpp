#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Lessons.h"
#include "Presets.h"

EQAnatomyProcessor::EQAnatomyProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "EQAnatomy", createLayout()),
      Thread ("EQ Anatomy IR builder")
{
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            apvts.addParameterListener (wp->paramID, this);

    const bool ja = juce::SystemStats::getUserLanguage().startsWithIgnoreCase ("ja")
                 || juce::SystemStats::getDisplayLanguage().startsWithIgnoreCase ("ja");
    uiState.setProperty ("lang", ja ? "ja" : "en", nullptr);
    uiState.setProperty ("learn", true, nullptr);
    uiState.setProperty ("lesson", 0, nullptr);
    uiState.setProperty ("slot", 0, nullptr);
    uiState.setProperty ("view", 0, nullptr);       // 0 = phase, 1 = group delay
    uiState.setProperty ("split", false, nullptr);
    uiState.setProperty ("anPair", 0, nullptr); uiState.setProperty ("anA", 5, nullptr); uiState.setProperty ("anB", 1, nullptr);   // Lab 2 default: In / Out
    for (int c = 0; c < 2; ++c) { inPeak[(size_t) c] = 0; outPeak[(size_t) c] = 0; inRms[(size_t) c] = 0; outRms[(size_t) c] = 0; }
    chainA.reserve(); chainB.reserve();
    startThread (juce::Thread::Priority::low);
    startTimerHz (25);
}

EQAnatomyProcessor::~EQAnatomyProcessor()
{
    stopTimer();
    LinkRegistry::remove (linkTap);
    signalThreadShouldExit(); buildEvent.signal(); stopThread (2000);
}

bool EQAnatomyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo() && sc != juce::AudioChannelSet::mono()) return false;
    }
    return true;
}

bool EQAnatomyProcessor::bandIsLin (int i) const
{
    return currentMode() == eqcore::Mode::Linear || apvts.getRawParameterValue (pid::band (i, "phase"))->load() > 0.5f;
}
bool EQAnatomyProcessor::dynAvailable (int band) const
{
    return currentMode() == eqcore::Mode::ZeroLatency && ! bandIsLin (band);
}

std::array<IIRChain::Target, eqcore::kBands> EQAnatomyProcessor::targets() const
{
    std::array<IIRChain::Target, eqcore::kBands> t {};
    const bool zero = currentMode() == eqcore::Mode::ZeroLatency;
    for (int i = 0; i < eqcore::kBands; ++i)
    {
        auto v = [&] (const char* w) { return apvts.getRawParameterValue (pid::band (i, w))->load(); };
        const int ch = (int) v ("ch");
        const int mask = (! zero || v ("phase") > 0.5f) ? 0 : (ch == 0 ? 3 : ch == 1 ? 1 : ch == 2 ? 2 : ch == 3 ? 4 : 8);   // only zero-latency min bands run as IIR
        t[(size_t) i] = { v ("on") > 0.5f, (eqcore::Type) (int) v ("type"), (int) v ("slope"), (double) v ("freq"), (double) v ("gain"), (double) v ("q"), mask };
    }
    return t;
}

std::array<eqcore::Band, eqcore::kBands> EQAnatomyProcessor::currentBands() const
{
    std::array<eqcore::Band, eqcore::kBands> b {};
    for (int i = 0; i < eqcore::kBands; ++i)
    {
        auto v = [&] (const char* w) { return apvts.getRawParameterValue (pid::band (i, w))->load(); };
        auto& x = b[(size_t) i];
        x.on = v ("on") > 0.5f; x.type = (eqcore::Type) (int) v ("type"); x.f = v ("freq"); x.g = v ("gain"); x.q = v ("q");
        x.slope = (int) v ("slope"); x.lin = v ("phase") > 0.5f; x.ch = (int) v ("ch");
    }
    return b;
}
eqcore::Mode  EQAnatomyProcessor::currentMode() const  { return (eqcore::Mode) (int) apvts.getRawParameterValue (pid::mode)->load(); }
eqcore::QMode EQAnatomyProcessor::currentQMode() const { return (eqcore::QMode) (int) apvts.getRawParameterValue (pid::qmode)->load(); }
int EQAnatomyProcessor::currentRes() const { return (int) apvts.getRawParameterValue (pid::res)->load(); }

void EQAnatomyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; blockSize = samplesPerBlock; linkTap->fs = sampleRate;
    convolution.prepare (sampleRate, samplesPerBlock);
    earConv.prepare (sampleRate, samplesPerBlock); loadEarKernel();
    ms.setSize (2, samplesPerBlock); dryD.setSize (2, samplesPerBlock); tmp.setSize (2, samplesPerBlock);
    for (auto& d : dryDelay) d.prepare (eqcore::linearTaps (2) / 2 + 64);
    for (auto& d : postDelay) d.prepare (256);
    maskDelay.prepare (eqcore::linearTaps (2) / 2 + 64 + 512 + ResSuppressor::latency());
    rs.prepare (sampleRate); for (auto& d : rsDelay) d.prepare (ResSuppressor::latency() + 8);
    svC = 1.0 - std::exp (-1.0 / (sampleRate * 0.003)); svG[0] = 1.0; for (int k = 1; k < 5; ++k) svG[k] = 0.0;
    maskBuf.assign ((size_t) samplesPerBlock, 0.0); maskDel.assign ((size_t) samplesPerBlock, 0.0);
    sat.prepare (sampleRate, samplesPerBlock);
    for (auto& d : det) d.reset();
    trans.prepare (sampleRate);
    detEq.prepare (sampleRate);
    dynNow.fill (0.0);
    detAtt = 1.0 - std::exp (-1.0 / (sampleRate * 0.005));
    detRel = 1.0 - std::exp (-1.0 / (sampleRate * 0.120));
    rmsCoef = 1.0 - std::exp (-1.0 / (sampleRate * 0.3));
    outGain.reset (sampleRate, 0.03); autoGain.reset (sampleRate, 0.05);
    xfadeLen = (int) (0.03 * sampleRate); modeFadeLen = (int) (0.015 * sampleRate);
    const auto t = targets();
    chainA.configure (t, currentQMode(), sampleRate); cur = &chainA; nxt = &chainB; xfade = 0;
    activeMode = (int) currentMode(); fadingOut = false; modeFade = modeFadeLen;
    activeKernelLat = wantedKernelLat.load(); activeSatLat = wantedSatLat.load(); activeRsLat = wantedRsLat.load();
    lastSolo = -1;
    dirty = true;
}

void EQAnatomyProcessor::loadEarKernel()
{
    const auto& k = earB.load() ? earKB : earKA;
    if (! k.empty()) earConv.loadImpulseResponse (k.data(), k.data(), (int) k.size());   // crossfades from the other one
}

void EQAnatomyProcessor::setEar (bool on, EarQuestion q)
{
    { const juce::SpinLock::ScopedLockType l (earLock); earQ = q; earDirty = true; }
    // level-match the question so loudness does not give the answer away
    eqcore::Band b; b.on = true; b.type = eqcore::Type::Bell; b.f = q.f; b.g = q.g; b.q = q.q;
    std::array<eqcore::Band, eqcore::kBands> bs {}; bs[0] = b;
    earGainComp = (float) eqcore::matchGain (eqcore::Model (bs, eqcore::QMode::Constant, getSampleRateOrDefault()), eqcore::Mode::ZeroLatency);
    if (on && q.kind == 1)
    {
        // pre-ring question: identical magnitude, min phase vs linear phase. No level matching needed (same magnitude).
        eqcore::Band c; c.on = true; c.type = (eqcore::Type) q.type; c.f = q.f; c.g = q.g; c.q = q.q; c.slope = q.slope;
        std::array<eqcore::Band, eqcore::kBands> cs {}; cs[0] = c;
        const eqcore::Model m (cs, eqcore::QMode::Constant, getSampleRateOrDefault());
        const auto lin = eqcore::linearPhaseFIR (m, 2 * kEarPreLat);                 // centre at kEarPreLat
        auto mn = eqcore::minimumPhaseFIR (m, eqcore::kMinPhaseLen);
        std::vector<float> L (lin.begin(), lin.end()), M ((size_t) kEarPreLat, 0.0f);  // min phase delayed to line up
        M.insert (M.end(), mn.begin(), mn.end());
        L.resize (M.size(), 0.0f);
        earKA = q.linOnB ? M : L; earKB = q.linOnB ? L : M;
        earGainComp = 1.0f;
        earKind = 1;
        loadEarKernel();
    }
    else earKind = 0;
    earOn = on;
    dirty = true;                                  // latency: + kEarPreLat while a pre-ring question plays
}

void EQAnatomyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples(), nch = std::min (2, main.getNumChannels());
    if (nch == 0 || n == 0) return;
    if (ms.getNumSamples() < n) { ms.setSize (2, n, false, false, true); dryD.setSize (2, n, false, false, true); tmp.setSize (2, n, false, false, true); }
    if ((int) maskBuf.size() < n) { maskBuf.resize ((size_t) n, 0.0); maskDel.resize ((size_t) n, 0.0); }

    // sidechain
    const float* scL = nullptr; const float* scR = nullptr;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) { scL = sc.getReadPointer (0); scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : scL; }
    }
    scConnected = scL != nullptr;

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const bool bypass = P (pid::bypass) > 0.5f, delta = P (pid::delta) > 0.5f, solo = P (pid::preSolo) > 0.5f, agOn = P (pid::autoGain) > 0.5f;
    const bool ear = earOn.load();
    outGain.setTargetValue (juce::Decibels::decibelsToGain (P (pid::output)));
    autoGain.setTargetValue (ear ? (earB.load() ? earGainComp.load() : 1.0f)
                                 : (agOn && ! bypass && ! delta && ! solo ? juce::Decibels::decibelsToGain (autoGainDb.load()) : 1.0f));

    auto* L = main.getWritePointer (0);
    auto* R = nch > 1 ? main.getWritePointer (1) : nullptr;

    // input meters
    {
        float pk[2] = { 0, 0 };
        for (int i = 0; i < n; ++i)
        {
            const float l = L[i], r = R ? R[i] : l;
            pk[0] = std::max (pk[0], std::abs (l)); pk[1] = std::max (pk[1], std::abs (r));
            inMs[0] += (l * l - inMs[0]) * rmsCoef; inMs[1] += (r * r - inMs[1]) * rmsCoef;
        }
        for (int c = 0; c < 2; ++c)
        {
            float o = inPeak[(size_t) c].load(); while (pk[c] > o && ! inPeak[(size_t) c].compare_exchange_weak (o, pk[c])) {}
            inRms[(size_t) c] = (float) inMs[c];
        }
    }

    // structure / targets for the zero-latency engine
    const auto t = targets(); const auto qm = currentQMode();
    if (cur->sameStructure (t, qm)) cur->setTargets (t);
    else if (xfade == 0) { nxt->configure (t, qm, sr); xfade = 1; }
    if (xfade > 0) nxt->setTargets (t);

    // mode / latency switch: fade out, switch, fade back in
    const int wantM = (int) currentMode(), wantK = wantedKernelLat.load(), wantS = wantedSatLat.load(), wantR = wantedRsLat.load();
    wantedMode = wantM;
    if ((wantM != activeMode || wantK != activeKernelLat || wantS != activeSatLat || wantR != activeRsLat) && ! fadingOut) fadingOut = true;

    // dry, delayed by the kernel latency (for Bypass / Delta / ear training)
    for (int c = 0; c < 2; ++c)
    {
        const float* src = c == 0 ? L : (R ? R : L);
        auto* d = dryD.getWritePointer (c);
        for (int i = 0; i < n; ++i) d[i] = dryDelay[c].process (src[i], activeKernelLat);
    }

    // encode to mid / side
    auto* M = ms.getWritePointer (0); auto* S = ms.getWritePointer (1);
    for (int i = 0; i < n; ++i) { const float l = L[i], r = R ? R[i] : l; M[i] = 0.5f * (l + r); S[i] = 0.5f * (l - r); }

    // per-band dynamics and transient / body weighting (zero-latency min bands only)
    std::array<bool, eqcore::kBands> dynOn {}; std::array<int, eqcore::kBands> dynCh {}; std::array<bool, eqcore::kBands> dynSC {}; std::array<int, eqcore::kBands> dynSrc {};
    std::array<float, eqcore::kBands> thr {}, above {}, below {}, tbT {}, tbB {}; bool anyDyn = false, anyTB = false;
    std::array<double, eqcore::kBands> attC {}, relC {}, kneeDb {};      // per-band detector speed and soft-knee width
    // A skewed range does not round-trip its default exactly (5 ms comes back as 4.9999995). Snapping to 0.01 makes the default
    // exactly the old fixed value, so sessions made before these controls existed sound bit-for-bit the same.
    auto snap = [] (float v) { return std::round ((double) v * 100.0) / 100.0; };
    trans.sens = snap (P (pid::tbSens)); trans.width = snap (P (pid::tbWidth)); trans.setAttack (snap (P (pid::tbAtt)));
    trans.method = (int) P (pid::tbMethod);
    detEq.set (P (pid::tbLc), P (pid::tbE1f), P (pid::tbE1g), P (pid::tbE1q), P (pid::tbE2f), P (pid::tbE2g), P (pid::tbE2q));
    for (int b = 0; b < eqcore::kBands; ++b)
    {
        auto v = [&] (const char* w) { return apvts.getRawParameterValue (pid::band (b, w))->load(); };
        const auto ty = (eqcore::Type) (int) v ("type");
        const bool minZero = activeMode == 0 && v ("on") > 0.5f && v ("phase") < 0.5f;
        dynOn[(size_t) b] = minZero && v ("dyn") > 0.5f
                         && (ty == eqcore::Type::Bell || ty == eqcore::Type::LowShelf || ty == eqcore::Type::HighShelf);
        { const int c = (int) v ("ch"); dynCh[(size_t) b] = c == 2 ? 1 : c == 3 ? 2 : c == 4 ? 3 : 0; }   // detector on M, S, L or R
        dynSrc[(size_t) b] = (int) v ("src");                       // 0 Input, 1 Sidechain, 2 Mid, 3 Side
        dynSC[(size_t) b] = dynSrc[(size_t) b] == 1 && scL != nullptr;
        attC[(size_t) b] = 1.0 - std::exp (-1.0 / (sr * (snap (v ("att")) / 1000.0)));
        relC[(size_t) b] = 1.0 - std::exp (-1.0 / (sr * (snap (v ("rel")) / 1000.0)));
        kneeDb[(size_t) b] = snap (v ("knee"));
        thr[(size_t) b] = v ("thr"); above[(size_t) b] = v ("range"); below[(size_t) b] = v ("below");
        if (dynOn[(size_t) b]) { det[(size_t) b].design (ty, v ("freq"), ty == eqcore::Type::Bell ? v ("q") : 0.707, sr); anyDyn = true; }
        else { det[(size_t) b].reset(); dynNow[(size_t) b] = 0.0; }
        const float tb = minZero ? v ("tb") : 0.0f;
        // −100 = attack only (left, TRANSIENT) … 0 = both … +100 = body only (right, BODY)
        tbT[(size_t) b] = tb > 0 ? 1.0f - tb / 100.0f : 1.0f;      // weight on the attack
        tbB[(size_t) b] = tb < 0 ? 1.0f + tb / 100.0f : 1.0f;      // weight on the body
        if (std::abs (tb) > 0.5f) anyTB = true;
    }
    if (! anyDyn) { cur->setDyn (dynNow); nxt->setDyn (dynNow); }

    // Transient mask (0 = body … 1 = attack) of the input mid. It runs whenever a band weights it or
    // the analyzer wants Transient / Body. The delayed copy lines up with the output (Lin / saturator latency).
    const int svSel = soloView.load();
    const bool svBlocked = ear || bypass || delta || solo || soloBand.load() >= 0;
    const int svTarget = svBlocked ? 0 : svSel;
    const bool needMask = anyTB || analyzerNeedTB.load() || svTarget >= 3 || svG[3] > 1e-4 || svG[4] > 1e-4;
    if (scopeNeed.load())
    {
        const auto w = scopeRawW.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) scopeRaw[(size_t) ((w + (juce::uint64) i) & (juce::uint64) (scopeRawN - 1))] = M[i];
        scopeRawW.store (w + (juce::uint64) n, std::memory_order_release);
    }
    if (needMask)
    {
        const int dly = activeKernelLat + activeSatLat + activeRsLat;
        for (int i = 0; i < n; ++i)
        {
            maskBuf[(size_t) i] = trans.process (detEq.active() ? detEq.process ((double) M[i]) : (double) M[i]);
            maskDel[(size_t) i] = (double) maskDelay.process ((float) maskBuf[(size_t) i], dly);
        }
    }

    std::array<double, eqcore::kBands> w; w.fill (1.0);
    if (activeMode == 0)
    {
        for (int i = 0; i < n; ++i)
        {
            if (anyDyn)
                for (int b = 0; b < eqcore::kBands; ++b)
                    if (dynOn[(size_t) b])
                    {
                        // detector input: sidechain, or the input's Mid / Side (a fixed one for Mid / Side, else the band's own channel)
                        const double x = dynSC[(size_t) b] ? 0.5 * ((double) scL[i] + (double) scR[i])
                                       : dynSrc[(size_t) b] == 2 ? (double) M[i]
                                       : dynSrc[(size_t) b] == 3 ? (double) S[i]
                                       : dynCh[(size_t) b] == 1 ? (double) S[i] : dynCh[(size_t) b] == 2 ? (double) M[i] + (double) S[i]
                                       : dynCh[(size_t) b] == 3 ? (double) M[i] - (double) S[i] : (double) M[i];
                        det[(size_t) b].process (x, attC[(size_t) b], relC[(size_t) b]);
                    }
            if (anyTB)
            {
                const double m = maskBuf[(size_t) i];
                for (int b = 0; b < eqcore::kBands; ++b) w[(size_t) b] = tbB[(size_t) b] + (tbT[(size_t) b] - tbB[(size_t) b]) * m;
            }
            if (subCounter++ % 32 == 0)
            {
                if (anyDyn)
                {
                    // ramp ("knee"): nothing at the threshold, the full range one knee width beyond it (12 dB by default; 0 = hard switch).
                    // Below the threshold: fades out under −60 dB so silence is not pulled up.
                    for (int b = 0; b < eqcore::kBands; ++b)
                        if (dynOn[(size_t) b])
                        {
                            const double lv = det[(size_t) b].levelDb(), th = thr[(size_t) b], kn = kneeDb[(size_t) b];
                            const double up = kn < 0.05 ? (lv > th ? 1.0 : 0.0) : juce::jlimit (0.0, 1.0, (lv - th) / kn);
                            const double dn = (kn < 0.05 ? (lv < th ? 1.0 : 0.0) : juce::jlimit (0.0, 1.0, (th - lv) / kn)) * juce::jlimit (0.0, 1.0, (lv + 72.0) / 12.0);
                            dynNow[(size_t) b] = above[(size_t) b] * up + below[(size_t) b] * dn;
                        }
                    cur->setDyn (dynNow); nxt->setDyn (dynNow);
                }
                cur->updateCoefficients (true); if (xfade > 0) nxt->updateCoefficients (true);
            }
            double v[2] = { M[i], S[i] };
            if (xfade > 0)
            {
                double v2[2] = { M[i], S[i] };
                cur->process (v, w); nxt->process (v2, w);
                const double a = (double) xfade / (double) xfadeLen;
                v[0] = v[0] * (1 - a) + v2[0] * a; v[1] = v[1] * (1 - a) + v2[1] * a;
                if (++xfade > xfadeLen) { std::swap (cur, nxt); xfade = 0; }
            }
            else cur->process (v, w);
            M[i] = (float) v[0]; S[i] = (float) v[1];
        }
    }
    else cur->updateCoefficients (true);    // keep following the knobs so a switch back is seamless
    for (int b = 0; b < eqcore::kBands; ++b)
    {
        dynGainDb[(size_t) b] = (float) dynNow[(size_t) b];
        detLevelDb[(size_t) b] = dynOn[(size_t) b] ? (float) det[(size_t) b].levelDb() : -120.0f;
    }

    // FIR part: linear-phase bands, or the whole EQ in Analog / Linear
    const bool needConv = activeMode != 0 || activeKernelLat > 0;
    if (needConv)
    {
        convolution.process (ms.getWritePointer (0), ms.getWritePointer (1), n);
    }
    else if (solo) { ms.clear (0, n); }                // a min-phase filter has nothing before the sound

    // decode, then Bypass / Delta / ear training
    const auto* dl = dryD.getReadPointer (0); const auto* dr = dryD.getReadPointer (1);
    auto* oL = tmp.getWritePointer (0); auto* oR = tmp.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        float l = M[i] + S[i], r = M[i] - S[i];
        if (ear || bypass) { l = dl[i]; r = dr[i]; }
        else if (delta) { l -= dl[i]; r -= dr[i]; }
        oL[i] = l; oR[i] = r;
    }

    // ear training: the question's bell on the delayed dry
    if (ear)
    {
        if (earDirty)
        {
            const juce::SpinLock::ScopedLockType l (earLock);
            eqcore::Section s { eqcore::Sec::Bell, std::pow (10.0, earQ.g / 40.0), earQ.q, earQ.f };
            const auto c = eqcore::digitalSection (s, sr);
            for (auto& f : earF) { f.b0 = c.b0; f.b1 = c.b1; f.b2 = c.b2; f.a1 = c.a1; f.a2 = c.a2; }
            earDirty = false;
        }
        const bool playEq = earB.load();
        if (earKind.load() == 1) earConv.process (oL, oR, n);       // pre-ring question: A and B both run through the convolver
        else
        for (int i = 0; i < n; ++i)
        {
            const double l = earF[0].process (oL[i]), r = earF[1].process (oR[i]);
            if (playEq) { oL[i] = (float) l; oR[i] = (float) r; }
        }
    }

    // resonance suppressor, after the EQ and before the saturator (left out of Bypass / Delta / Pre-ring solo / ear training)
    {
        rs.setParams (P (pid::rsDepth), P (pid::rsThr), P (pid::rsLo), P (pid::rsHi), P (pid::rsSel), P (pid::rsAtt), P (pid::rsRel), P (pid::rsDelta) > 0.5f);
        const bool run = activeRsLat > 0 && P (pid::rsOn) > 0.5f && ! bypass && ! delta && ! solo && ! ear;
        if (run) { if (! rsWasOn) rs.reset(); rs.process (oL, oR, n); }
        else
        {
            if (rsWasOn) for (auto& d : rs.dispDb) d = 0.0f;
            if (activeRsLat > 0) for (int i = 0; i < n; ++i) { oL[i] = rsDelay[0].process (oL[i], activeRsLat); oR[i] = rsDelay[1].process (oR[i], activeRsLat); }
        }
        rsWasOn = run;
    }

    // band saturator, after the EQ (left out of Bypass / Delta / Pre-ring solo / ear training)
    {
        const bool satOn = P (pid::satOn) > 0.5f;
        sat.setOversampling ((int) P (pid::satOS));
        sat.setTargets (P (pid::satLo), P (pid::satHi), P (pid::satDrive), P (pid::satMix), (int) P (pid::satType), P (pid::satAuto) > 0.5f);
        const bool run = satOn && ! bypass && ! delta && ! solo && ! ear;
        if (run)
        {
            if (! satWasOn) sat.reset();
            sat.process (oL, oR, n, P (pid::satDelta) > 0.5f);
            satMakeupDb = sat.currentMakeupDb();
        }
        else if (activeSatLat > 0)          // keep the reported latency honest when the saturator is skipped
            for (int i = 0; i < n; ++i) { oL[i] = postDelay[0].process (oL[i], activeSatLat); oR[i] = postDelay[1].process (oR[i], activeSatLat); }
        satWasOn = run;
    }

    // band solo: listen through a band-pass at the band
    {
        const int sb = soloBand.load();
        if (sb >= 0 && ! ear)
        {
            {
                auto v = [&] (const char* w) { return apvts.getRawParameterValue (pid::band (sb, w))->load(); };
                const auto ty = (eqcore::Type) (int) v ("type");
                const double f = juce::jlimit (20.0, sr * 0.45, (double) v ("freq"));
                const double q = (ty == eqcore::Type::Bell || ty == eqcore::Type::Notch || ty == eqcore::Type::AllPass) ? std::max (0.7, (double) v ("q")) : 0.7;
                const double wv = 2 * juce::MathConstants<double>::pi * f / sr, cw = std::cos (wv), al = std::sin (wv) / (2 * q), a0 = 1 + al;
                for (auto& fl : soloF) { fl.b0 = al / a0; fl.b1 = 0; fl.b2 = -al / a0; fl.a1 = -2 * cw / a0; fl.a2 = (1 - al) / a0; if (sb != lastSolo) fl.z1 = fl.z2 = 0; }
                lastSolo = sb;
            }
            for (int i = 0; i < n; ++i) { oL[i] = (float) soloF[0].process (oL[i]); oR[i] = (float) soloF[1].process (oR[i]); }
        }
        else lastSolo = -1;
    }

    // gains, mode-switch fade, output
    for (int i = 0; i < n; ++i)
    {
        if (fadingOut)
        {
            if (--modeFade <= 0)
            {
                modeFade = 0; fadingOut = false;
                activeMode = wantedMode.load(); activeKernelLat = wantedKernelLat.load(); activeSatLat = wantedSatLat.load(); activeRsLat = wantedRsLat.load();
                for (auto& d : rsDelay) d.clear(); rs.reset();
                for (auto& d : dryDelay) d.clear();
                for (auto& d : postDelay) d.clear();
                maskDelay.clear();
            }
        }
        else if (modeFade < modeFadeLen) ++modeFade;
        const float env = (float) modeFade / (float) modeFadeLen;
        const float g = outGain.getNextValue() * autoGain.getNextValue() * env;
        oL[i] *= g; oR[i] *= g;
    }
    // analyzer feed: the finished output, before any solo view, so the display stays the same while you listen to M / S / T / B
    // (the four buffers are written together: Mid, Side, Transient, Body — T / B are silent unless the editor asks)
    {
        int s1, n1, s2, n2; analyzerFifo.prepareToWrite (n, s1, n1, s2, n2);
        const bool tb = needMask && analyzerNeedTB.load();
        auto feed = [&] (int start, int count, int from)
        {
            for (int i = 0; i < count; ++i)
            {
                const float m = 0.5f * (oL[from + i] + oR[from + i]), s = 0.5f * (oL[from + i] - oR[from + i]);
                const float k = tb ? (float) maskDel[(size_t) (from + i)] : 0.0f;
                analyzerBuffer[(size_t) (start + i)] = m;
                analyzerS[(size_t) (start + i)] = s;
                analyzerT[(size_t) (start + i)] = tb ? m * k : 0.0f;
                analyzerB[(size_t) (start + i)] = tb ? m * (1.0f - k) : 0.0f;
                const float il = dryD.getSample (0, from + i), ir = dryD.getSample (1, from + i);
                analyzerInM[(size_t) (start + i)] = 0.5f * (il + ir);
                analyzerInS[(size_t) (start + i)] = 0.5f * (il - ir);
                analyzerSC[(size_t) (start + i)] = scL != nullptr ? 0.5f * (scL[from + i] + scR[from + i]) : 0.0f;
            }
        };
        feed (s1, n1, 0); feed (s2, n2, n1);
        analyzerFifo.finishedWrite (n1 + n2);
    }
    linkTap->push (oL, oR, n);       // the same signal, for the other instances' analyzers

    // solo view: listen to the finished output's Mid, Side, or its Transient / Body part (output · mask, output · (1 − mask)).
    // It yields to EAR, Bypass, Delta, Pre-ring solo and band solo. Moves are cross-faded (~15 ms).
    if (svTarget != 0 || svG[0] < 0.99999)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < 5; ++k) svG[k] += ((k == svTarget ? 1.0 : 0.0) - svG[k]) * svC;
            const double l = oL[i], r = oR[i], m = 0.5 * (l + r), s = 0.5 * (l - r);
            const double kk = (svG[3] > 1e-6 || svG[4] > 1e-6) ? maskDel[(size_t) i] : 0.0;
            oL[i] = (float) (svG[0] * l + svG[1] * m + svG[2] * s + svG[3] * l * kk + svG[4] * l * (1.0 - kk));
            oR[i] = (float) (svG[0] * r + svG[1] * m - svG[2] * s + svG[3] * r * kk + svG[4] * r * (1.0 - kk));
        }
        if (svTarget == 0 && svG[0] >= 0.99999) { svG[0] = 1.0; for (int k = 1; k < 5; ++k) svG[k] = 0.0; }
    }
    {
        float pk[2] = { 0, 0 };
        for (int i = 0; i < n; ++i)
        {
            const double l = oL[i], r = oR[i];
            pk[0] = std::max (pk[0], (float) std::abs (l)); pk[1] = std::max (pk[1], (float) std::abs (r));
            outMs[0] += (l * l - outMs[0]) * rmsCoef; outMs[1] += (r * r - outMs[1]) * rmsCoef;
            cLR += (l * r - cLR) * rmsCoef; cLL += (l * l - cLL) * rmsCoef; cRR += (r * r - cRR) * rmsCoef;
        }
        for (int c = 0; c < 2; ++c)
        {
            float o = outPeak[(size_t) c].load(); while (pk[c] > o && ! outPeak[(size_t) c].compare_exchange_weak (o, pk[c])) {}
            outRms[(size_t) c] = (float) outMs[c];
        }
        correlation = (cLL > 1e-9 && cRR > 1e-9) ? (float) (cLR / std::sqrt (cLL * cRR)) : 1.0f;
    }
    if (R) { main.copyFrom (0, 0, tmp, 0, 0, n); main.copyFrom (1, 0, tmp, 1, 0, n); }
    else for (int i = 0; i < n; ++i) L[i] = 0.5f * (oL[i] + oR[i]);

}

void EQAnatomyProcessor::computeLatency()
{
    const auto mode = currentMode();
    bool lin = mode == eqcore::Mode::Linear;
    if (! lin) for (int i = 0; i < eqcore::kBands; ++i) lin = lin || (apvts.getRawParameterValue (pid::band (i, "on"))->load() > 0.5f && bandIsLin (i));
    const int k = lin ? eqcore::linearTaps (currentRes()) / 2 : 0;
    const bool satOn = apvts.getRawParameterValue (pid::satOn)->load() > 0.5f;
    sat.setOversampling ((int) apvts.getRawParameterValue (pid::satOS)->load());
    const int s = satOn ? sat.latencySamples() : 0;
    const int rl = apvts.getRawParameterValue (pid::rsOn)->load() > 0.5f ? ResSuppressor::latency() : 0;
    wantedKernelLat = k; wantedSatLat = s; wantedRsLat = rl;
    const int e = earOn.load() && earKind.load() == 1 ? kEarPreLat : 0;     // the pre-ring question's convolver (after the EQ path)
    if (k + s + rl + e != latencyReported) { latencyReported = k + s + rl + e; setLatencySamples (latencyReported); }
}

void EQAnatomyProcessor::timerCallback()
{
    if (! dirty.exchange (false)) return;
    const auto mode = currentMode();
    const auto cm = eqcore::channelModel (currentBands(), currentQMode(), getSampleRateOrDefault(), mode, 1);
    autoGainDb = (float) juce::Decibels::gainToDecibels (eqcore::matchGain (cm));
    computeLatency();
    ++buildRequest; buildEvent.signal();
}

void EQAnatomyProcessor::run()
{
    int done = 0;
    while (! threadShouldExit())
    {
        buildEvent.wait (500);
        if (threadShouldExit()) break;
        while (done != buildRequest.load() && ! threadShouldExit())
        {
            done = buildRequest.load();
            const double fs = getSampleRateOrDefault();
            const auto mode = currentMode(); const int res = currentRes();
            const auto bands = currentBands(); const auto qm = currentQMode();
            const auto cmM = eqcore::channelModel (bands, qm, fs, mode, 1);
            const auto cmS = eqcore::channelModel (bands, qm, fs, mode, 2);
            auto kM = eqcore::buildKernel (cmM, res), kS = eqcore::buildKernel (cmS, res);
            // the two channels share one latency: an empty side kernel becomes a delayed impulse
            const int lat = std::max (kM.latency, kS.latency);
            auto fill = [&] (eqcore::Kernel& k, const eqcore::ChannelModel& cm)
            {
                if (k.active && k.latency == lat) return;
                if (! k.active || k.latency != lat)
                {
                    const int N = eqcore::linearTaps (res);
                    juce::ignoreUnused (N);
                    if (! k.active) { k.h.assign ((size_t) lat + 64, 0.0); k.h[(size_t) lat] = 1.0; }
                    else { std::vector<double> h ((size_t) lat, 0.0); h.insert (h.end(), k.h.begin(), k.h.end()); k.h = h; }   // min-phase only
                    k.active = true; k.latency = lat;
                }
                juce::ignoreUnused (cm);
            };
            bool any = kM.active || kS.active || eqcore::lrInFir (bands, mode);
            if (any) { fill (kM, cmM); fill (kS, cmS); }
            const bool load = any || mode != eqcore::Mode::ZeroLatency;      // Analog / Linear always convolve
            if (! any && load) { kM.h.assign (64, 0.0); kM.h[0] = 1.0; kM.active = true; kS = kM; }

            // impulse view: the mid channel (or side when only side bands exist)
            const bool showSide = cmM.minM.empty() && cmM.linM.empty() && ! (cmS.minM.empty() && cmS.linM.empty());
            const auto& cmv = showSide ? cmS : cmM;
            auto imp = eqcore::impulse (cmv, showSide ? kS : kM);
            if (! any && mode == eqcore::Mode::ZeroLatency) imp.latency = 0;
            const auto pr = eqcore::preRinging (imp.h, imp.latency, fs);
            {
                DisplayIR d; d.h.assign (imp.h.begin(), imp.h.end()); d.latency = imp.latency; d.preDb = pr.db; d.preMs = pr.ms; d.fs = fs;
                d.version = done;
                const juce::SpinLock::ScopedLockType l (irLock); displayIR = std::move (d);
            }
            displayVersionAtomic = done;

            if (load)
            {
                const bool solo = apvts.getRawParameterValue (pid::preSolo)->load() > 0.5f;
                const size_t len = std::max (kM.h.size(), kS.h.size());
                juce::AudioBuffer<float> b (2, (int) len); b.clear();
                for (int c = 0; c < 2; ++c)
                {
                    auto h = c == 0 ? kM.h : kS.h;
                    if (solo)      // keep only what comes before the main tap
                    {
                        const int end = std::max (0, lat - (int) std::round (fs * 0.00025));
                        const int fade = std::min (end / 4, (int) (fs * 0.002));
                        for (int i = 0; i < (int) h.size(); ++i)
                            h[(size_t) i] = i >= end ? 0.0 : h[(size_t) i] * (i >= end - fade && fade > 0 ? (double) (end - i) / fade : 1.0);
                    }
                    for (size_t i = 0; i < h.size(); ++i) b.setSample (c, (int) i, (float) h[i]);
                }
                if (eqcore::lrInFir (bands, mode))
                {
                    // L/R bands: four kernels (two of them cross terms), about twice the convolution CPU
                    auto k4 = eqcore::buildKernel4 (bands, qm, fs, mode, res);
                    const int L4 = (int) k4.h[0].size();
                    juce::AudioBuffer<float> q (4, L4); q.clear();
                    for (int c = 0; c < 4; ++c)
                    {
                        auto& h = k4.h[(size_t) c];
                        if (solo)
                        {
                            const int end = std::max (0, k4.latency - (int) std::round (fs * 0.00025));
                            const int fade = std::min (end / 4, (int) (fs * 0.002));
                            for (int i = 0; i < (int) h.size(); ++i)
                                h[(size_t) i] = i >= end ? 0.0 : h[(size_t) i] * (i >= end - fade && fade > 0 ? (double) (end - i) / fade : 1.0);
                        }
                        for (int i = 0; i < L4; ++i) q.setSample (c, i, (float) h[(size_t) i]);
                    }
                    convolution.loadImpulseResponse (q.getReadPointer (0), q.getReadPointer (1), L4, q.getReadPointer (2), q.getReadPointer (3));
                }
                else
                    convolution.loadImpulseResponse (b.getReadPointer (0), b.getReadPointer (1), b.getNumSamples());
            }
        }
    }
}

// ---------------------------------------------------------------------------
//  A / B / C / D and morph
// ---------------------------------------------------------------------------
void EQAnatomyProcessor::storeCurrentSlot()
{
    const auto key = juce::String ("slot") + juce::String (currentSlot());
    uiState.removeChild (uiState.getChildWithName (key), nullptr);
    juce::ValueTree saved (key); saved.addChild (apvts.copyState(), -1, nullptr); uiState.addChild (saved, -1, nullptr);
}
juce::ValueTree EQAnatomyProcessor::slotState (int slot) const
{
    auto t = uiState.getChildWithName (juce::String ("slot") + juce::String (slot));
    return t.isValid() && t.getNumChildren() > 0 ? t.getChild (0) : juce::ValueTree();
}
void EQAnatomyProcessor::switchSlot (int slot)
{
    if (slot == currentSlot()) return;
    storeCurrentSlot();
    auto other = slotState (slot);
    undo.beginNewTransaction();
    if (other.isValid())
    {
        // set parameter by parameter so it lands on the undo history and the host
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                auto node = other.getChildWithProperty ("id", rp->paramID);
                if (node.isValid()) rp->setValueNotifyingHost (rp->convertTo0to1 ((float) node.getProperty ("value")));
            }
    }
    uiState.setProperty ("slot", slot, nullptr);
    dirty = true;
}
void EQAnatomyProcessor::morph (int from, int to, float tt)
{
    auto a = slotState (from), b = slotState (to);
    if (! a.isValid() || ! b.isValid()) return;
    const float t = juce::jlimit (0.0f, 1.0f, tt);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto& id = rp->paramID;
            if (id == pid::bypass || id == pid::delta || id == pid::preSolo) continue;
            auto na = a.getChildWithProperty ("id", id), nb = b.getChildWithProperty ("id", id);
            if (! na.isValid() || ! nb.isValid()) continue;
            const float va = na.getProperty ("value"), vb = nb.getProperty ("value");
            float v;
            const bool discrete = dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr || dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr;
            if (discrete) v = t < 0.5f ? va : vb;
            else if (id.endsWith ("_freq") || id.endsWith ("_q") || id.endsWith ("_att") || id.endsWith ("_rel") || id == pid::satLo || id == pid::satHi)
                v = std::exp (std::log (std::max (1e-3f, va)) * (1 - t) + std::log (std::max (1e-3f, vb)) * t);   // log for frequency and Q
            else v = va + (vb - va) * t;
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
        }
    dirty = true;
}

void EQAnatomyProcessor::resetToFlat()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    soloBand = -1;
}

void EQAnatomyProcessor::applyPreset (int index)
{
    undo.beginNewTransaction();
    const auto& P = presets();
    auto apply = [this] (const PresetSlot& s)
    {
        resetToFlat();
        for (auto& [id, v] : s.values)
            if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    // clear stored slots
    for (int k = 0; k < 4; ++k) uiState.removeChild (uiState.getChildWithName (juce::String ("slot") + juce::String (k)), nullptr);
    if (index < 0 || index >= (int) P.size())
    {
        resetToFlat();
        uiState.setProperty ("preset", -1, nullptr);
        uiState.setProperty ("slot", 0, nullptr);
        dirty = true; return;
    }
    const auto& pr = P[(size_t) index];
    // fill B–D first (each stored as a snapshot), then leave slot A live
    for (int k = (int) pr.slots.size() - 1; k >= 0; --k)
    {
        apply (pr.slots[(size_t) k]);
        const auto key = juce::String ("slot") + juce::String (k);
        juce::ValueTree saved (key); saved.addChild (apvts.copyState(), -1, nullptr); uiState.addChild (saved, -1, nullptr);
    }
    uiState.setProperty ("slot", 0, nullptr);
    uiState.setProperty ("morphTo", 1, nullptr);
    uiState.setProperty ("preset", index, nullptr);
    uiState.setProperty ("learn", false, nullptr);
    uiState.setProperty ("popup", true, nullptr);
    if (pr.focus >= 0) uiState.setProperty ("selected", pr.focus, nullptr);
    uiState.setProperty ("focusUntil", juce::Time::getMillisecondCounterHiRes() + 8000.0, nullptr);
    dirty = true;
}

void EQAnatomyProcessor::applyLesson (int index)
{
    const auto& L = lessons();
    if (index < 0 || index >= (int) L.size()) return;
    const auto& ls = L[(size_t) index];
    undo.beginNewTransaction();
    auto set = [this] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
    };
    for (int i = 0; i < eqcore::kBands; ++i)
    {
        set (pid::band (i, "phase"), 0); set (pid::band (i, "ch"), 0); set (pid::band (i, "tb"), 0);
        set (pid::band (i, "below"), 0); set (pid::band (i, "src"), 0);
        set (pid::band (i, "att"), 5.0f); set (pid::band (i, "rel"), 120.0f); set (pid::band (i, "knee"), 12.0f);   // lessons use the original detector
        if (i < (int) ls.bands.size())
        {
            const auto& b = ls.bands[(size_t) i];
            set (pid::band (i, "on"), b.on ? 1.0f : 0.0f); set (pid::band (i, "type"), (float) (int) b.type);
            set (pid::band (i, "freq"), (float) b.f); set (pid::band (i, "gain"), (float) b.g);
            set (pid::band (i, "q"), (float) b.q); set (pid::band (i, "slope"), (float) b.slope);
            set (pid::band (i, "dyn"), b.dyn ? 1.0f : 0.0f);
            if (b.dyn) { set (pid::band (i, "thr"), b.thr); set (pid::band (i, "range"), b.range); set (pid::band (i, "below"), b.below); set (pid::band (i, "src"), (float) b.src); }
            set (pid::band (i, "phase"), b.lin ? 1.0f : 0.0f); set (pid::band (i, "ch"), (float) b.ch); set (pid::band (i, "tb"), b.tb);
        }
        else
        {
            const auto& d = bandDefault (i);
            set (pid::band (i, "on"), 0.0f); set (pid::band (i, "type"), (float) (int) d.type);
            set (pid::band (i, "freq"), (float) d.f); set (pid::band (i, "gain"), 0.0f); set (pid::band (i, "q"), (float) d.q);
            set (pid::band (i, "dyn"), 0.0f);
        }
    }
    set (pid::tbSens, 0.0f); set (pid::tbWidth, 6.0f); set (pid::tbAtt, 2.0f); set (pid::tbMethod, 1.0f); set (pid::tbLc, 20.0f); set (pid::tbE1g, 0.0f); set (pid::tbE2g, 0.0f);
    set (pid::mode, (float) ls.mode);
    if (ls.res >= 0) set (pid::res, (float) ls.res);
    set (pid::qmode, (float) ls.qmode);
    set (pid::bypass, 0); set (pid::delta, 0); set (pid::preSolo, 0);
    set (pid::satOn, ls.sat.on ? 1.0f : 0.0f); set (pid::satDelta, 0);
    if (ls.sat.on)
    {
        set (pid::satType, (float) ls.sat.type); set (pid::satLo, ls.sat.lo); set (pid::satHi, ls.sat.hi);
        set (pid::satDrive, ls.sat.drive); set (pid::satMix, ls.sat.mix); set (pid::satOS, (float) ls.sat.os);
    }
    uiState.setProperty ("lesson", index, nullptr);
    uiState.setProperty ("selected", ls.select, nullptr);
}

void EQAnatomyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("EQAnatomyState");
    root.addChild (apvts.copyState(), -1, nullptr);
    root.addChild (uiState.createCopy(), -1, nullptr);
    if (auto xml = root.createXml()) copyXmlToBinary (*xml, dest);
}

void EQAnatomyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto root = juce::ValueTree::fromXml (*xml);
        auto p = root.getChildWithName (apvts.state.getType());
        if (p.isValid())
        {
            // sessions saved before 0.1.1 may lack the detector method: they were Ratio, keep them that way
            bool hasMethod = false;
            for (int i = 0; i < p.getNumChildren(); ++i)
                if (p.getChild (i).getProperty ("id").toString() == pid::tbMethod) { hasMethod = true; break; }
            if (! hasMethod)
            {
                juce::ValueTree n ("PARAM"); n.setProperty ("id", juce::String (pid::tbMethod), nullptr); n.setProperty ("value", 0.0f, nullptr);
                p.appendChild (n, nullptr);
            }
            apvts.replaceState (p);
        }
        auto u = root.getChildWithName ("UI");
        if (u.isValid()) uiState.copyPropertiesAndChildrenFrom (u, nullptr);
        undo.clearUndoHistory();
        soloView = 0;                       // solo state is never loaded
        dirty = true;
    }
}

juce::AudioProcessorEditor* EQAnatomyProcessor::createEditor() { return new EQAnatomyEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new EQAnatomyProcessor(); }
