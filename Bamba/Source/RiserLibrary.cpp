#include "RiserLibrary.h"
#include "../../Common/BackgroundJob.h"

const char* RiserLibrary::kindName (int kind)
{
    static const char* const names[] = { "RISE", "DOWN", "SWEEP", "IMPACT", "REVERSE" };
    return juce::isPositiveAndBelow (kind, (int) NumKinds) ? names[kind] : "ALL";
}

bool RiserLibrary::Entry::matches (int kindFilter, int lengthFilter) const
{
    if (kindFilter >= 0 && kind != kindFilter)
        return false;
    if (lengthFilter == Short) return seconds < shortSeconds;
    if (lengthFilter == Long)  return seconds >= shortSeconds;
    return true;
}

RiserLibrary::RiserLibrary()
{
    juce::File saved;
    if (auto settings = openSettings())
        saved = juce::File (settings->getValue ("riserFolder"));
    folder = saved.isDirectory() ? saved : defaultFolder();
    rescan();
}

RiserLibrary::~RiserLibrary()
{
    if (job != nullptr)
        job->cancelled = true; // the job notices and stops; nothing here waits for it
}

std::unique_ptr<juce::PropertiesFile> RiserLibrary::openSettings() const
{
    juce::PropertiesFile::Options options;
    options.applicationName = "BAMBA";
    options.folderName = "BAMBA";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (options);
}

juce::File RiserLibrary::defaultFolder()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    for (auto f : { docs.getChildFile ("Splice").getChildFile ("Samples"), docs.getChildFile ("Splice"),
                    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Splice") })
        if (f.isDirectory())
            return f;
    return docs;
}

juce::File RiserLibrary::getFolder() const
{
    const juce::ScopedLock sl (lock);
    return folder;
}

void RiserLibrary::setFolder (const juce::File& newFolder)
{
    {
        const juce::ScopedLock sl (lock);
        folder = newFolder;
    }
    if (auto settings = openSettings())
    {
        settings->setValue ("riserFolder", newFolder.getFullPathName());
        settings->saveIfNeeded();
    }
    rescan();
}

void RiserLibrary::rescan()
{
    if (job != nullptr)
        job->cancelled = true;
    job = std::make_shared<Job>();
    ++shared->scans;
    skazi::runDetached ([s = shared, j = job, root = getFolder()]
    {
        auto found = scan (root, [&j] { return j->cancelled.load(); });
        if (! j->cancelled)
        {
            const juce::ScopedLock sl (s->lock);
            s->entries = std::move (found);
            ++s->version;
        }
        --s->scans;
    });
}

std::vector<RiserLibrary::Entry> RiserLibrary::getEntries() const
{
    const juce::ScopedLock sl (shared->lock);
    return shared->entries;
}

int RiserLibrary::classify (const juce::File& f)
{
    // Whole words for the short, ambiguous ones ("rise", "hit"); anywhere for the long ones ("uplifter").
    auto words = [] (const juce::String& s)
    {
        auto t = juce::StringArray::fromTokens (s.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 _-.()"), " _-.()", {});
        t.removeEmptyStrings();
        return t;
    };
    auto any = [] (const juce::String& s, std::initializer_list<const char*> parts)
    {
        for (auto* p : parts) if (s.contains (p)) return true;
        return false;
    };
    auto hasWord = [] (const juce::StringArray& w, std::initializer_list<const char*> list)
    {
        for (auto* x : list) if (w.contains (x)) return true;
        return false;
    };

    auto kindOf = [&] (const juce::String& text) -> int
    {
        const auto s = text.toLowerCase();
        const auto w = words (text);
        if (any (s, { "downlift", "down_lift", "downsweep", "down_sweep", "sweep_down", "sweepdown", "downer", "falling", "faller" })
            || (hasWord (w, { "down", "fall" }) && hasWord (w, { "fx", "sfx", "sweep", "woosh", "whoosh", "noise", "riser", "pitch" }))
            || hasWord (w, { "downlifter", "downlifters", "downsweeps" }))
            return Down;
        if (any (s, { "reverse", "reversed", "swell" }) || hasWord (w, { "rvs", "rev", "rvrs", "suck", "suckback" }))
            return Reverse;
        if (any (s, { "impact", "subdrop", "sub_drop", "explosion", "slam" })
            || hasWord (w, { "boom", "booms" }) || (hasWord (w, { "hit", "hits" }) && hasWord (w, { "fx", "sfx", "cinematic" })))
            return Impact;
        if (any (s, { "riser", "uplifter", "upsweep", "up_sweep", "sweep_up", "sweepup", "buildup", "build_up", "build-up", "rising", "tension", "climb", "snare_roll", "snareroll" })
            || hasWord (w, { "rise", "rises", "lift", "lifter", "uplift", "uplifts", "build", "builds", "snareroll", "riser" }))
            return Rise;
        if (any (s, { "whoosh", "woosh", "swoosh", "sweep", "white_noise", "whitenoise", "transition" }) || hasWord (w, { "swoop", "zap" }))
            return Sweep;
        return -1;
    };

    const auto name = f.getFileNameWithoutExtension();
    const auto path = f.getFullPathName().toLowerCase();
    const auto nameWords = words (name);

    // No kicks, drum hits or MIDI renders; a riser word has to be in the name or the folder above it.
    if (hasWord (nameWords, { "kick", "kik", "hat", "hihat", "clap", "snare" }) && ! any (name.toLowerCase(), { "roll", "build", "riser", "fx" }))
        return -1;
    if (path.contains ("midi") && ! path.contains ("audiomidi"))
        return -1;

    const int byName = kindOf (name);
    if (byName >= 0)
        return byName;
    // "FX/Risers/SPLC_042.wav": a numbered file inside a riser folder
    const auto parent = f.getParentDirectory().getFileName();
    if (any (parent.toLowerCase(), { "fx", "sfx", "riser", "uplift", "impact", "sweep", "reverse", "transition", "build" }))
        return kindOf (parent);
    return -1;
}

std::vector<RiserLibrary::Entry> RiserLibrary::scan (const juce::File& root, const std::function<bool()>& shouldStop)
{
    std::vector<Entry> found;
    if (! root.isDirectory())
        return found;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    for (const auto& item : juce::RangedDirectoryIterator (root, true, "*.wav;*.aif;*.aiff;*.flac"))
    {
        if (shouldStop && shouldStop())
            return {};

        const auto& f = item.getFile();
        const int kind = classify (f);
        if (kind < 0)
            continue;

        // The length comes from the header alone, so scanning stays quick.
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
        if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
            continue;
        const float seconds = (float) ((double) reader->lengthInSamples / reader->sampleRate);
        if (seconds < 0.1f || seconds > 120.0f)
            continue;

        // Splice keeps packs under ...\Samples\packs\<Pack Name>\...; otherwise use the first sub-folder.
        auto parts = juce::StringArray::fromTokens (f.getParentDirectory().getRelativePathFrom (root), "\\/", {});
        parts.removeEmptyStrings();
        const int packsAt = parts.indexOf ("packs", true);
        juce::String pack = packsAt >= 0 && packsAt + 1 < parts.size() ? parts[packsAt + 1]
                          : parts.isEmpty() || parts[0] == ".." ? root.getFileName() : parts[0];
        found.push_back ({ f, pack, kind, seconds });
    }

    std::sort (found.begin(), found.end(), [] (const Entry& a, const Entry& b)
    {
        return a.seconds != b.seconds ? a.seconds < b.seconds : a.file.getFileName().compareNatural (b.file.getFileName()) < 0;
    });
    return found;
}
