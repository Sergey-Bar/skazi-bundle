#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

/** The user's own kick one-shots (their Splice downloads by default), found by
    scanning a folder in the background. Nothing is copied or bundled: the plugin
    plays the files where they are. One library is shared by every KICKASS instance;
    get it with juce::SharedResourcePointer<KickLibrary>. */
class KickLibrary
{
public:
    struct Entry
    {
        juce::File file;
        juce::String pack; // the Splice pack (or first sub-folder) it came from
    };

    KickLibrary();
    ~KickLibrary();

    juce::File getFolder() const;
    /** Remembers the folder for next time and rescans it. */
    void setFolder (const juce::File&);
    void rescan();

    bool isScanning() const { return shared->scans.load() > 0; }
    /** Bumped after every scan, so the editor knows to refresh. */
    int getVersion() const { return shared->version.load(); }

    std::vector<Entry> getEntries() const;
    int indexOf (const juce::File&) const;

    /** Kick-named one-shots only: no loops, no "no kick" loops, nothing long. */
    static bool looksLikeKickOneShot (const juce::File&);
    /** Scans a folder tree for kick one-shots, sorted by pack then name. */
    static std::vector<Entry> scanKicks (const juce::File& root, const std::function<bool()>& shouldStop);

private:
    // Scans run as detached jobs (Common/BackgroundJob.h): closing the plugin never waits for a slow
    // folder or cloud drive. Results land in this shared block, which outlives the library if it must.
    struct Shared
    {
        juce::CriticalSection lock;
        std::vector<Entry> entries;
        std::atomic<int> version { 0 }, scans { 0 };
    };
    struct Job { std::atomic<bool> cancelled { false }; };
    static juce::File defaultFolder();
    std::unique_ptr<juce::PropertiesFile> openSettings() const;

    mutable juce::CriticalSection lock;
    juce::File folder;
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    std::shared_ptr<Job> job;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickLibrary)
};
