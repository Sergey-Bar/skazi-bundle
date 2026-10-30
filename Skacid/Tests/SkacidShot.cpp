// Renders the SKACID editor to a PNG without a host, after playing the pattern for a moment (a held key runs it).
//   SkacidShot <out.png> [preset index | random]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "skacid.png");

    SkacidProcessor proc;
    const double sr = 48000.0;
    proc.setRateAndBufferSizeDetails (sr, 512);
    proc.prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    if (argc > 2)
    {
        if (juce::String (argv[2]) == "random") proc.randomizeAll();
        else proc.loadPreset (juce::String (argv[2]).getIntValue());
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    juce::AudioBuffer<float> buf (2, 512);
    for (int pos = 0; pos < (int) sr * 2 + 3000; pos += 512)
    {
        juce::MidiBuffer midi;
        if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "could not write %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    return 0;
}
