#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

// =============================================================================
//  Skins. Six looks, from the design canvas plus the current one:
//    0 Studio        current: graphite, amber accent, yellow dotted phase
//    1 Lab Light     A. Lab: light paper, teal accent (web lab look)
//    2 Studio Night  B. Studio: deeper black, brighter amber, higher contrast
//    3 Bridge        Meter A. Bridge: warm charcoal, cream VU-face graph
//    4 Blueprint     Meter B. Lab: blue-grey paper, bronze accent
//    5 Lesson Warm   B改 / A改: warm dark with the lesson card's amber
//  The colours are variables so the whole editor can switch at run time.
// =============================================================================
namespace theme
{
inline juce::Colour bg, header, panel, well, line, grid, zero, text, muted, faint, accent, onAccent, pre, phase,
                    lessonBg, lessonLn, sat, ok, side, analyzer, popup, live, splitWell, curve;
inline bool lightPanels = false;
inline int current = 0;
inline std::array<juce::uint32, 8> bands { 0xff8f86e6, 0xff55b7c5, 0xff7cc66f, 0xffe48f5c, 0xffef7a6e, 0xffde8fbd, 0xffc9b458, 0xff8aa0d8 };

inline int numSkins() { return 14; }
inline const char* skinName (int i)
{
    static const char* n[] = { "Studio", "Lab Light", "Studio Night", "Bridge", "Blueprint", "Lesson Warm",
                               "Frame Graphite", "Frame Navy", "Bright Edge", "Frame Silver",
                               "Soft Grey", "Soft Sand", "Soft Sage", "Soft Dusk" };
    return n[juce::jlimit (0, 13, i)];
}

inline void setSkin (int i)
{
    current = juce::jlimit (0, 13, i);
    struct P { juce::uint32 bg, header, panel, well, line, grid, zero, text, muted, faint, accent, onAccent, pre, phase,
               lessonBg, lessonLn, sat, ok, side, analyzer, popup, live, splitWell; bool light; std::array<juce::uint32, 8> b; };
    static const std::array<juce::uint32, 8> darkBands { 0xff8f86e6, 0xff55b7c5, 0xff7cc66f, 0xffe48f5c, 0xffef7a6e, 0xffde8fbd, 0xffc9b458, 0xff8aa0d8 };
    static const std::array<juce::uint32, 8> inkBands  { 0xff6b5bb5, 0xff0d7385, 0xff3c8a3a, 0xffc47410, 0xffb83f36, 0xffa3487e, 0xff8a7414, 0xff3d5aa8 };
    static const P p[] = {
        { 0xff121417, 0xff191c21, 0xff191c21, 0xff0d0f12, 0xff2a2e35, 0xff1d2126, 0xff353a42, 0xffe4e1da, 0xff8d929a, 0xff6c727b, 0xfff0b34a, 0xff1a1306, 0xffef7a6e, 0xffe9cf6a,
          0xff1b1a16, 0xff5a4620, 0xffe8a33a, 0xff8fc779, 0xff6fc3d6, 0xff2a2e35, 0xff1c2026, 0xfffff1d6, 0xff0b0d10, false, darkBands },
        { 0xfff6f7f9, 0xffe9edf1, 0xffffffff, 0xfffbfcfd, 0xffc8cfd7, 0xffe6e9ed, 0xffb9c1ca, 0xff16202b, 0xff566271, 0xff7a8592, 0xff0d6575, 0xffffffff, 0xffb83f36, 0xffa8861c,
          0xfffbf3e8, 0xffe3c9a6, 0xffc47410, 0xff3c8a3a, 0xff2f7fb0, 0xffdfe4ea, 0xffffffff, 0xff16202b, 0xfff1f3f6, true, inkBands },
        { 0xff0b0c0e, 0xff121418, 0xff15181c, 0xff07080a, 0xff23272e, 0xff15191e, 0xff2c3139, 0xffeceae4, 0xff8d929a, 0xff646a73, 0xffffbf47, 0xff1a1306, 0xffff7f70, 0xfff2d86a,
          0xff16150f, 0xff4e3d1b, 0xffffaa3c, 0xff8fd27a, 0xff6fd0e6, 0xff20242b, 0xff121418, 0xfffff4dc, 0xff050607, false, darkBands },
        { 0xff17181a, 0xff1e1f22, 0xff26272a, 0xffefe4c8, 0xff3a3b3f, 0xffdccfb2, 0xffb8ad98, 0xffe8e3d8, 0xff8f8a80, 0xff7d776c, 0xffe8a33a, 0xff1b1205, 0xffc0392b, 0xff8a6a12,
          0xff26221a, 0xff5a4620, 0xffc4741a, 0xff4f8f5a, 0xff2f6f8a, 0xffe2d5b6, 0xff2a2621, 0xff2a2621, 0xffe6dabd, false, inkBands },
        { 0xffe1e6eb, 0xffd3dae2, 0xffeef1f4, 0xfff6f7f9, 0xffb9c1ca, 0xffdfe3e8, 0xffa9b2bd, 0xff16202b, 0xff566271, 0xff7a8592, 0xff8a5a1c, 0xfffbf3e8, 0xffb83f36, 0xff0d6575,
          0xfffbf3e8, 0xffe3c9a6, 0xffc47410, 0xff3c8a3a, 0xff0d6575, 0xffd5dce4, 0xffffffff, 0xff16202b, 0xffedf0f3, true, inkBands },
        { 0xff15140f, 0xff1d1b15, 0xff1f1d17, 0xff100f0b, 0xff34302a, 0xff1e1c16, 0xff3d382f, 0xffece5d4, 0xff9a9282, 0xff746d60, 0xffe8a33a, 0xff1b1205, 0xffef7a6e, 0xffe9cf6a,
          0xff241f14, 0xff6a5222, 0xffe8a33a, 0xff8fc779, 0xff7cc0c9, 0xff29261e, 0xff221f18, 0xfffff1d6, 0xff0c0b08, false, darkBands },
        // 6 Frame Graphite: bright panels and header on a dark graphite gap, silver edge
        { 0xff22252a, 0xffe9edf1, 0xffffffff, 0xfffbfcfd, 0xff9aa5b1, 0xffe6e9ed, 0xffb9c1ca, 0xff16202b, 0xff566271, 0xff7a8592, 0xff0d6575, 0xffffffff, 0xffb83f36, 0xffa8861c,
          0xfffbf3e8, 0xffe3c9a6, 0xffc47410, 0xff3c8a3a, 0xff2f7fb0, 0xffdfe4ea, 0xffffffff, 0xff16202b, 0xfff1f3f6, true, inkBands },
        // 7 Frame Navy: blue-grey paper panels on deep navy, bronze accent
        { 0xff14202e, 0xffd3dae2, 0xffeef1f4, 0xfff6f7f9, 0xff8d9db0, 0xffdfe3e8, 0xffa9b2bd, 0xff16202b, 0xff566271, 0xff7a8592, 0xff8a5a1c, 0xfffbf3e8, 0xffb83f36, 0xff0d6575,
          0xfffbf3e8, 0xffe3c9a6, 0xffc47410, 0xff3c8a3a, 0xff0d6575, 0xffd5dce4, 0xffffffff, 0xff16202b, 0xffedf0f3, true, inkBands },
        // 8 Bright Edge: Studio Night panels, near-black gap, bright cream edge lines
        { 0xff050607, 0xff121418, 0xff15181c, 0xff07080a, 0xffcfc8b4, 0xff15191e, 0xff2c3139, 0xffeceae4, 0xff8d929a, 0xff646a73, 0xffffbf47, 0xff1a1306, 0xffff7f70, 0xfff2d86a,
          0xff16150f, 0xff4e3d1b, 0xffffaa3c, 0xff8fd27a, 0xff6fd0e6, 0xff20242b, 0xff121418, 0xfffff4dc, 0xff050607, false, darkBands },
        // 9 Frame Silver: Frame Graphite inverted. Dark panels on a light silver gap
        { 0xffd7dbe0, 0xff191c21, 0xff191c21, 0xff0d0f12, 0xff0a0b0d, 0xff1d2126, 0xff353a42, 0xffe4e1da, 0xff8d929a, 0xff6c727b, 0xfff0b34a, 0xff1a1306, 0xffef7a6e, 0xffe9cf6a,
          0xff1b1a16, 0xff5a4620, 0xffe8a33a, 0xff8fc779, 0xff6fc3d6, 0xff2a2e35, 0xff1c2026, 0xfffff1d6, 0xff0b0d10, false, darkBands },
        // 10-13 Soft: light skins with lower brightness (less glare in a dim studio), dark ink on muted paper
        // 10 Soft Grey: neutral grey paper, teal accent
        { 0xffc9ced4, 0xffd2d6db, 0xffdadee2, 0xffe1e4e7, 0xffa8b0b9, 0xffcfd3d8, 0xff9aa3ad, 0xff1b232c, 0xff4f5a66, 0xff6b7581, 0xff0d6575, 0xffffffff, 0xffa5382f, 0xff8a6d12,
          0xffe8e0d2, 0xffc9b08c, 0xffad640d, 0xff357a33, 0xff2a6f9a, 0xffc4cad1, 0xffe4e7ea, 0xff1b232c, 0xffd6dade, true, inkBands },
        // 11 Soft Sand: warm beige paper, bronze accent
        { 0xffcfc8bb, 0xffd8d2c6, 0xffe0dacf, 0xffe7e2d8, 0xffb0a797, 0xffd6cfc2, 0xffa39a89, 0xff2a241b, 0xff5e5547, 0xff7a7060, 0xff8a5a1c, 0xfffbf3e8, 0xffa5382f, 0xff0d6575,
          0xffe9dfcc, 0xffc9ae84, 0xffad640d, 0xff4a7a33, 0xff2f6a7f, 0xffcbc3b4, 0xffebe6dd, 0xff2a241b, 0xffdcd5c9, true, inkBands },
        // 12 Soft Sage: green-grey paper, deep green accent
        { 0xffc5ccc6, 0xffcfd5d0, 0xffd8ddd9, 0xffdfe3e0, 0xffa2ada5, 0xffcbd2cd, 0xff95a199, 0xff1c2620, 0xff4c5a51, 0xff69766d, 0xff2f6b4f, 0xffffffff, 0xffa5382f, 0xff8a6d12,
          0xffe6e1d2, 0xffc4b48e, 0xffad640d, 0xff2f7a3a, 0xff2a6a8a, 0xffbfc7c1, 0xffe2e6e3, 0xff1c2620, 0xffd3d9d5, true, inkBands },
        // 13 Soft Dusk: blue-grey paper, slate-blue accent
        { 0xffbcc5d0, 0xffc7cfd8, 0xffd1d8e0, 0xffd9dfe6, 0xff98a5b4, 0xffc5cdd7, 0xff8d9aaa, 0xff18212d, 0xff47546a, 0xff657187, 0xff355e9c, 0xffffffff, 0xffa5382f, 0xff8a6d12,
          0xffe3ded4, 0xffbfae8e, 0xffad640d, 0xff357a33, 0xff1f6f8a, 0xffb8c2ce, 0xffdde3e9, 0xff18212d, 0xffcdd4dc, true, inkBands },
    };
    const auto& s = p[current];
    auto C = [] (juce::uint32 v) { return juce::Colour (v); };
    bg = C (s.bg); header = C (s.header); panel = C (s.panel); well = C (s.well); line = C (s.line); grid = C (s.grid); zero = C (s.zero);
    text = C (s.text); muted = C (s.muted); faint = C (s.faint); accent = C (s.accent); onAccent = C (s.onAccent); pre = C (s.pre); phase = C (s.phase);
    lessonBg = C (s.lessonBg); lessonLn = C (s.lessonLn); sat = C (s.sat); ok = C (s.ok); side = C (s.side); analyzer = C (s.analyzer);
    popup = C (s.popup); live = C (s.live); splitWell = C (s.splitWell); lightPanels = s.light; bands = s.b;
    curve = current == 3 ? juce::Colour (0xffb8620e) : accent;     // amber is too pale on the cream well
}

// =============================================================================
//  Surface: only the gaps (the window background between panels, buttons and
//  graphs). Independent of the skin, so 6 looks x 6 surfaces.
//    0 Flat   1 Brushed Silver   2 Gunmetal   3 Dots   4 Carbon   5 Pinstripe
//  Dots / Carbon / Pinstripe are drawn from the skin's own background colour.
// =============================================================================
inline int surface = 0;
inline int numSurfaces() { return 6; }
inline const char* surfaceName (int i)
{
    static const char* n[] = { "Flat", "Brushed Silver", "Gunmetal", "Dots", "Carbon", "Pinstripe" };
    return n[juce::jlimit (0, 5, i)];
}

inline juce::Image surfaceTile (int kind, juce::Colour base)
{
    static std::map<std::pair<int, juce::uint32>, juce::Image> cache;
    const auto key = std::make_pair (kind, base.getARGB());
    if (auto it = cache.find (key); it != cache.end()) return it->second;

    juce::Image img;
    if (kind == 1 || kind == 2)                       // brushed metal: horizontal streaks, wraps in both directions
    {
        const int W = 256, H = 256; img = juce::Image (juce::Image::ARGB, W, H, true);
        juce::Random rnd (kind * 7919);
        const float c0 = kind == 1 ? 0.80f : 0.17f, amp = kind == 1 ? 0.060f : 0.030f;
        for (int y = 0; y < H; ++y)
        {
            std::vector<float> row ((size_t) W);
            const float rowBias = (rnd.nextFloat() - 0.5f) * amp * 1.4f;
            for (auto& v : row) v = rnd.nextFloat() - 0.5f;
            for (int pass = 0; pass < 3; ++pass)      // wrapped low-pass makes the streaks long
                for (int x = 0; x < W; ++x) row[(size_t) x] = 0.5f * row[(size_t) x] + 0.25f * (row[(size_t) ((x + W - 1) % W)] + row[(size_t) ((x + 1) % W)]);
            for (int x = 0; x < W; ++x)
            {
                const float v = juce::jlimit (0.0f, 1.0f, c0 + rowBias + row[(size_t) x] * amp * 5.0f);
                img.setPixelAt (x, y, juce::Colour::fromFloatRGBA (v * 0.985f, v * 0.99f, v, 1.0f));
            }
        }
    }
    else if (kind == 3)                               // dot grid
    {
        const int N = 14; img = juce::Image (juce::Image::ARGB, N, N, true);
        juce::Graphics g (img);
        g.fillAll (base);
        g.setColour (base.getPerceivedBrightness() > 0.5f ? juce::Colours::black.withAlpha (0.13f) : juce::Colours::white.withAlpha (0.09f));
        g.fillEllipse (N * 0.5f - 1.1f, N * 0.5f - 1.1f, 2.2f, 2.2f);
    }
    else if (kind == 4)                               // carbon weave
    {
        const int N = 8; img = juce::Image (juce::Image::ARGB, N, N, true);
        const bool light = base.getPerceivedBrightness() > 0.5f;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
            {
                const bool horiz = ((x / 4) + (y / 4)) % 2 == 0;
                const float t = (horiz ? (float) (x % 4) : (float) (y % 4)) / 3.0f;
                img.setPixelAt (x, y, light ? base.darker (0.03f + 0.10f * t) : base.brighter (0.02f + 0.20f * t));
            }
    }
    else                                              // pinstripe
    {
        img = juce::Image (juce::Image::ARGB, 6, 4, true);
        const bool light = base.getPerceivedBrightness() > 0.5f;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 6; ++x)
                img.setPixelAt (x, y, x == 0 ? (light ? base.darker (0.08f) : base.brighter (0.14f))
                                              : x == 1 ? (light ? base.brighter (0.2f) : base.darker (0.25f)) : base);
    }
    cache[key] = img;
    return img;
}

inline void paintSurface (juce::Graphics& g, juce::Rectangle<int> area)
{
    if (surface == 0) { g.setColour (bg); g.fillRect (area); return; }
    // The finished background is rendered once per (surface, colour, size) and then only blitted,
    // so animated children above it never pay for the texture again.
    static juce::Image cached; static int kSurf = -1, kW = 0, kH = 0; static juce::uint32 kBg = 0;
    if (! cached.isValid() || kSurf != surface || kW != area.getWidth() || kH != area.getHeight() || kBg != bg.getARGB())
    {
        cached = juce::Image (juce::Image::RGB, juce::jmax (1, area.getWidth()), juce::jmax (1, area.getHeight()), false);
        juce::Graphics c (cached);
        const auto r = cached.getBounds();
        c.setTiledImageFill (surfaceTile (surface, bg), 0, 0, 1.0f);
        c.fillRect (r);
        if (surface <= 2)                             // a soft light fall-off so the metal does not look printed
        {
            c.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (surface == 1 ? 0.22f : 0.05f), 0.0f, 0.0f,
                                                     juce::Colours::black.withAlpha (surface == 1 ? 0.14f : 0.22f), 0.0f, (float) r.getBottom(), false));
            c.fillRect (r);
        }
        kSurf = surface; kW = area.getWidth(); kH = area.getHeight(); kBg = bg.getARGB();
    }
    g.drawImageAt (cached, area.getX(), area.getY());
}

// Text drawn on the graph well (which is cream in Bridge)
inline juce::Colour onWell() { return well.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff3a342a) : muted; }
// A soft shade or highlight that works on light and dark panels
inline juce::Colour shade (float a)  { return lightPanels ? juce::Colours::black.withAlpha (a * 0.25f) : juce::Colours::black.withAlpha (a); }
inline juce::Colour hilite (float a) { return lightPanels ? juce::Colours::black.withAlpha (a * 0.6f) : juce::Colours::white.withAlpha (a); }

inline juce::Colour bandColour (int i) { return juce::Colour (bands[(size_t) juce::jlimit (0, 7, i)]); }

inline juce::Font ui (float size, bool bold = false)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Hiragino Sans", size, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
   #endif
}
inline juce::Font mono (float size, bool bold = false)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions ("Menlo", size, bold ? juce::Font::bold : juce::Font::plain));
   #else
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
   #endif
}

inline const juce::Colour res (0xffd862b4);      // resonance suppressor
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel() { apply(); }
    void apply()
    {
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, onAccent);
        setColour (juce::ComboBox::backgroundColourId, lightPanels ? juce::Colours::white : panel.brighter (0.06f));
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, muted);
        setColour (juce::PopupMenu::backgroundColourId, popup);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, muted);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, onAccent);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Label::textWhenEditingColourId, text);
        setColour (juce::TextEditor::backgroundColourId, well);
        setColour (juce::TextEditor::textColourId, lightPanels || well.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff16202b) : text);
        setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::Slider::backgroundColourId, shade (0.4f));
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if (b.getProperties().getWithDefault ("selector", false))
        {
            g.setColour ((over || down) ? panel.brighter (0.10f) : panel); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (accent.withAlpha (0.75f)); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
            return;
        }
        auto col = b.findColour (on ? juce::TextButton::buttonOnColourId : juce::TextButton::buttonColourId);
        if (! on && (over || down)) col = lightPanels ? col.darker (0.06f) : col.brighter (0.12f);
        g.setColour (col); g.fillRoundedRectangle (r, 4.0f);
        if (! on) { g.setColour (b.findColour (juce::ComboBox::outlineColourId)); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
    }
    juce::Font getTextButtonFont (juce::TextButton& b, int h) override
    {
        const float px = (float) (double) b.getProperties().getWithDefault ("fontPx", 12.0);   // a button may ask for a smaller font
        return ui (juce::jmin (px, h * 0.5f));
    }
    // a button that opens a menu or a panel carries "chevron" (1 = down, 2 = up) and gets the same line arrow as the menus (ComboBox)
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool down) override
    {
        const int ch = (int) b.getProperties().getWithDefault ("chevron", 0);
        if (ch == 0 && b.getProperties().getWithDefault ("selector", false))
        {
            g.setFont (getTextButtonFont (b, b.getHeight())); g.setColour (accent);
            g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
            return;
        }
        if (ch == 0) { juce::LookAndFeel_V4::drawButtonText (g, b, over, down); return; }
        auto r = b.getLocalBounds();
        const bool textless = b.getButtonText().isEmpty();
        auto az = textless ? r.toFloat() : r.removeFromRight (20).toFloat();
        if (! textless)
        {
            g.setFont (getTextButtonFont (b, b.getHeight()));
            g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
            g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (6), juce::Justification::centred, 1);
        }
        const float cx = az.getCentreX() - (textless ? 0.0f : 2.0f), cy = az.getCentreY(), w = 4.5f, h = 2.5f * (ch == 2 ? -1.0f : 1.0f);
        juce::Path p; p.startNewSubPath (cx - w, cy - h); p.lineTo (cx, cy + h); p.lineTo (cx + w, cy - h);
        g.setColour (b.findColour (juce::ComboBox::arrowColourId).withAlpha (b.isEnabled() ? 0.9f : 0.2f));
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return ui (11.5f); }
    juce::Font getPopupMenuFont() override { return ui (13.0f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

// make sure the variables hold the default skin before anything paints
inline const bool initialised = [] { setSkin (0); return true; }();
}
