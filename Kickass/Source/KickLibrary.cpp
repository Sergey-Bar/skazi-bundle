#include "KickLibrary.h"
#include "../../Common/BackgroundJob.h"

KickLibrary::KickLibrary()
{
    juce::File saved;
    if (auto settings = openSettings())
        saved = juce::File (settings->getValue ("kickFolder"));
    folder = saved.isDirectory() ? saved : defaultFolder();
    rescan();
}

KickLibrary::~KickLibrary()
{
    if (job != nullptr)
        job->cancelled = true; // the job notices and stops; nothing here waits for it
}

std::unique_ptr<juce::PropertiesFile> KickLibrary::openSettings() const
{
    juce::PropertiesFile::Options options;
    options.applicationName = "KICKASS";
    options.folderName = "KICKASS";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (options);
}

juce::File KickLibrary::defaultFolder()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    for (auto f : { docs.getChildFile ("Splice").getChildFile ("Samples"), docs.getChildFile ("Splice"),
                    juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Splice") })
        if (f.isDirectory())
            return f;
    return docs;
}

juce::File KickLibrary::getFolder() const
{
    const juce::ScopedLock sl (lock);
    return folder;
}

void KickLibrary::setFolder (const juce::File& newFolder)
{
    {
        const juce::ScopedLock sl (lock);
        folder = newFolder;
    }
    if (auto settings = openSettings())
    {
        settings->setValue ("kickFolder", newFolder.getFullPathName());
        settings->saveIfNeeded();
    }
    rescan();
}

void KickLibrary::rescan()
{
    if (job != nullptr)
        job->cancelled = true;
    job = std::make_shared<Job>();
    ++shared->scans;
    skazi::runDetached ([s = shared, j = job, root = getFolder()]
    {
        auto found = scanKicks (root, [&j] { return j->cancelled.load(); });
        if (! j->cancelled)
        {
            const juce::ScopedLock sl (s->lock);
            s->entries = std::move (found);
            ++s->version;
        }
        --s->scans;
    });
}

std::vector<KickLibrary::Entry> KickLibrary::getEntries() const
{
    const juce::ScopedLock sl (shared->lock);
    return shared->entries;
}

int KickLibrary::indexOf (const juce::File& f) const
{
    const juce::ScopedLock sl (shared->lock);
    const auto& entries = shared->entries;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].file == f)
            return (int) i;
    return -1;
}

bool KickLibrary::looksLikeKickOneShot (const juce::File& f)
{
    const auto name = f.getFileName().toLowerCase();
    const auto path = f.getFullPathName().toLowerCase();
    auto isKickWord = [] (const juce::String& s)
    {
        return s.contains ("kick") || s.contains ("kik") || s.contains ("bassdrum") || s.contains ("bass_drum");
    };
    // A kick by name, or anything inside a kick folder ("Kicks/", "Tuned Kicks/", "01_Kick/").
    const bool kick = isKickWord (name) || isKickWord (f.getParentDirectory().getFileName().toLowerCase());
    const bool loop = path.contains ("loop") || path.contains ("no_kick") || path.contains ("nokick")
                   || path.contains ("no kick") || path.contains ("kick free") || path.contains ("kick_free")
                   || path.contains ("without") || name.contains ("bpm"); // tempo in the name means a loop or beat
    return kick && ! loop && f.getSize() < 1500000; // ~8 s of 24-bit stereo; one-shots are far shorter
}

std::vector<KickLibrary::Entry> KickLibrary::scanKicks (const juce::File& root, const std::function<bool()>& shouldStop)
{
    std::vector<Entry> found;

    for (const auto& item : juce::RangedDirectoryIterator (root, true, "*.wav;*.aif;*.aiff;*.flac"))
    {
        if (shouldStop())
            return found;

        const auto& f = item.getFile();
        if (! looksLikeKickOneShot (f))
            continue;

        // Splice keeps packs under ...\Samples\packs\<Pack Name>\...; otherwise use the first sub-folder.
        auto parts = juce::StringArray::fromTokens (f.getParentDirectory().getRelativePathFrom (root), "\\/", {});
        parts.removeEmptyStrings();
        const int packsAt = parts.indexOf ("packs", true);
        juce::String pack = packsAt >= 0 && packsAt + 1 < parts.size() ? parts[packsAt + 1]
                          : parts.isEmpty() ? root.getFileName() : parts[0];
        found.push_back ({ f, pack });
    }

    std::sort (found.begin(), found.end(), [] (const Entry& a, const Entry& b)
    {
        const int byPack = a.pack.compareNatural (b.pack);
        return byPack != 0 ? byPack < 0 : a.file.getFileName().compareNatural (b.file.getFileName()) < 0;
    });

    return found;
}
