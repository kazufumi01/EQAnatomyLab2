#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =============================================================================
//  Whole-window zoom. The view is laid out once at its design size and drawn
//  through a scale transform, so every part grows together and stays sharp
//  (it is vector drawing, re-rendered at the new size, not a bitmap stretch).
//  Zoom comes from the window corner or the % menu in the header; it is saved
//  with the session and remembered as the default for new instances.
// =============================================================================
struct ZoomPrefs
{
    static juce::PropertiesFile& file()
    {
        static juce::PropertiesFile f ([] { juce::PropertiesFile::Options o;
                                            o.applicationName = "JPK Anatomy Plugins"; o.filenameSuffix = ".settings";
                                            o.osxLibrarySubFolder = "Application Support"; o.folderName = "JPK";
                                            return o; }());
        return f;
    }
    static float load (const juce::String& key) { return (float) file().getDoubleValue (key, 1.0); }
    static void save (const juce::String& key, float z) { file().setValue (key, (double) z); file().saveIfNeeded(); }
};

inline const juce::Array<float>& zoomSteps() { static const juce::Array<float> z { 0.75f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f }; return z; }

// A small "100%" button for the header
class ZoomButton : public juce::TextButton
{
public:
    std::function<float()> get;
    std::function<void (float)> set;
    // optional skins (EQ Anatomy Lab): names, current index and setter
    juce::StringArray skinNames; std::function<int()> getSkin; std::function<void (int)> setSkin;
    juce::StringArray surfaceNames; std::function<int()> getSurface; std::function<void (int)> setSurface;
    juce::StringArray scaleNames; std::function<int()> getScale; std::function<void (int)> setScale;   // graph vertical scale
    ZoomButton() { onClick = [this] { showMenu(); }; setTooltip ("Window size and look"); }
    void refresh() { if (get) setButtonText (juce::String (juce::roundToInt (get() * 100.0f)) + "%"); }
private:
    void showMenu()
    {
        juce::PopupMenu m; const float now = get ? get() : 1.0f;
        const bool skins = setSkin && skinNames.size() > 0;
        if (skins) m.addSectionHeader ("Size");
        for (int i = 0; i < zoomSteps().size(); ++i)
        {
            const float z = zoomSteps()[i];
            m.addItem (i + 1, juce::String (juce::roundToInt (z * 100.0f)) + "%", true, std::abs (z - now) < 0.01f);
        }
        if (skins)
        {
            m.addSectionHeader ("Look");
            const int cur = getSkin ? getSkin() : 0;
            for (int i = 0; i < skinNames.size(); ++i) m.addItem (100 + i, skinNames[i], true, i == cur);
        }
        if (setSurface && surfaceNames.size() > 0)
        {
            m.addSectionHeader ("Surface (gaps only)");
            const int cur = getSurface ? getSurface() : 0;
            for (int i = 0; i < surfaceNames.size(); ++i) m.addItem (200 + i, surfaceNames[i], true, i == cur);
        }
        if (setScale && scaleNames.size() > 0)
        {
            m.addSectionHeader ("Graph scale (dB)");
            const int cur = getScale ? getScale() : 0;
            for (int i = 0; i < scaleNames.size(); ++i) m.addItem (300 + i, scaleNames[i], true, i == cur);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this] (int r)
                         {
                             if (r >= 300 && setScale) setScale (r - 300);
                             else
                             if (r >= 200 && setSurface) setSurface (r - 200);
                             else if (r >= 100 && setSkin) setSkin (r - 100);
                             else if (r > 0 && r <= zoomSteps().size() && set) set (zoomSteps()[r - 1]);
                         });
    }
};

template <class View, class Proc>
class ScalingEditor : public juce::AudioProcessorEditor
{
public:
    ScalingEditor (Proc& p, int w, int h, const juce::String& prefKey)
        : juce::AudioProcessorEditor (p), proc (p), view (p), baseW (w), baseH (h), key (prefKey)
    {
        addAndMakeVisible (view);
        view.setBounds (0, 0, w, h);
        view.zoom.get = [this] { return zoom; };
        view.zoom.set = [this] (float z) { setZoom (z); };

        float z = proc.uiState.hasProperty ("zoom") ? (float) proc.uiState.getProperty ("zoom") : ZoomPrefs::load (key);
        z = juce::jlimit (0.5f, 2.5f, z);
        setResizable (true, true);
        setResizeLimits (juce::roundToInt (w * 0.5f), juce::roundToInt (h * 0.5f), juce::roundToInt (w * 2.5f), juce::roundToInt (h * 2.5f));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) w / (double) h);
        setZoom (z);
    }

    void setZoom (float z)
    {
        z = juce::jlimit (0.5f, 2.5f, z);
        setSize (juce::roundToInt (baseW * z), juce::roundToInt (baseH * z));
    }

    void resized() override
    {
        zoom = (float) getWidth() / (float) baseW;
        view.setTransform (juce::AffineTransform::scale (zoom));
        proc.uiState.setProperty ("zoom", zoom, nullptr);
        ZoomPrefs::save (key, zoom);
        view.zoom.refresh();
    }

private:
    Proc& proc;
    View view;
    int baseW, baseH; juce::String key; float zoom = 1.0f;
};
