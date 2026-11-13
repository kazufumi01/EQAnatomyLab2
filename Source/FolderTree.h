#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// =============================================================================
//  User folders for presets and curves (1.0). What is on disk is the truth:
//  folders made, renamed or moved in the Finder / Explorer show up the next
//  time a menu opens. Sub-folders come first, then the files, both in natural
//  order; hidden items (".name") are skipped.
// =============================================================================
namespace ftree
{
inline juce::Array<juce::File> sorted (juce::Array<juce::File> a)
{
    std::sort (a.begin(), a.end(), [] (const juce::File& x, const juce::File& y) { return x.getFileName().compareNatural (y.getFileName()) < 0; });
    return a;
}
inline juce::Array<juce::File> subDirs (const juce::File& dir)
{
    juce::Array<juce::File> out;
    for (auto& d : dir.findChildFiles (juce::File::findDirectories, false)) if (! d.getFileName().startsWithChar ('.')) out.add (d);
    return sorted (out);
}
inline juce::Array<juce::File> files (const juce::File& dir, const juce::String& pattern)
{
    juce::Array<juce::File> out;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, pattern)) if (! f.getFileName().startsWithChar ('.')) out.add (f);
    return sorted (out);
}
// every file below dir, in menu order
inline void collect (const juce::File& dir, const juce::String& pattern, juce::Array<juce::File>& out, int depth = 0)
{
    if (depth > 8) return;
    for (auto& d : subDirs (dir)) collect (d, pattern, out, depth + 1);
    out.addArray (files (dir, pattern));
}
inline juce::Array<juce::File> all (const juce::File& dir, const juce::String& pattern) { juce::Array<juce::File> a; collect (dir, pattern, a); return a; }
// a menu that mirrors the folders. Item id = base + index into `list` (filled here)
inline void addToMenu (juce::PopupMenu& m, const juce::File& dir, const juce::String& pattern, juce::Array<juce::File>& list, int base,
                       const std::function<bool (const juce::File&)>& ticked, const juce::String& emptyText, int depth = 0)
{
    if (depth > 8) return;
    for (auto& d : subDirs (dir))
    {
        juce::PopupMenu sm; addToMenu (sm, d, pattern, list, base, ticked, emptyText, depth + 1);
        if (sm.getNumItems() == 0) sm.addItem (-1, emptyText, false);
        m.addSubMenu (d.getFileName(), sm);
    }
    for (auto& f : files (dir, pattern)) { list.add (f); m.addItem (base + list.size() - 1, f.getFileNameWithoutExtension(), true, ticked && ticked (f)); }
}
// "" = the root, then "A", "A/B" …
inline juce::StringArray folders (const juce::File& root)
{
    juce::StringArray out { "" };
    std::function<void (const juce::File&, int)> walk = [&] (const juce::File& d, int depth)
    {
        if (depth > 8) return;
        for (auto& s : subDirs (d)) { out.add (s.getRelativePathFrom (root).replaceCharacter ('\\', '/')); walk (s, depth + 1); }
    };
    walk (root, 0);
    return out;
}
inline juce::File folderFor (const juce::File& root, const juce::String& rel, const juce::String& newSub)
{
    auto d = rel.isEmpty() ? root : root.getChildFile (rel);
    for (auto& part : juce::StringArray::fromTokens (newSub.trim(), "/", ""))
        if (part.trim().isNotEmpty()) d = d.getChildFile (juce::File::createLegalFileName (part.trim()));
    d.createDirectory();
    return d;
}
// the folder field of a save dialog: existing folders + a field for a new sub-folder
inline void addFolderFields (juce::AlertWindow& w, const juce::File& root, const juce::String& current, bool ja)
{
    auto fs = folders (root);
    juce::StringArray shown; for (auto& f : fs) shown.add (f.isEmpty() ? (ja ? juce::String::fromUTF8 ("（いちばん上）") : juce::String ("(top level)")) : f);
    w.addComboBox ("folder", shown, ja ? juce::String::fromUTF8 ("フォルダ") : juce::String ("Folder"));
    if (auto* cb = w.getComboBoxComponent ("folder")) cb->setSelectedItemIndex (std::max (0, fs.indexOf (current)));
    w.addTextEditor ("newFolder", {}, ja ? juce::String::fromUTF8 ("新しいフォルダ（空欄なら作らない。A/B で階層）") : juce::String ("New sub-folder (blank = none; A/B for levels)"));
}
inline juce::File chosenFolder (juce::AlertWindow& w, const juce::File& root)
{
    const auto fs = folders (root);
    int i = 0; if (auto* cb = w.getComboBoxComponent ("folder")) i = cb->getSelectedItemIndex();
    return folderFor (root, fs[juce::jlimit (0, fs.size() - 1, i)], w.getTextEditorContents ("newFolder"));
}
inline juce::String relFolder (const juce::File& root, const juce::File& file)
{
    const auto p = file.getParentDirectory();
    return p == root || ! p.isAChildOf (root) ? juce::String() : p.getRelativePathFrom (root).replaceCharacter ('\\', '/');
}
}
