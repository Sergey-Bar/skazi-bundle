// Renders the TBON 303 editor to PNGs without a host, one per tab (the saved "uiTab" picks the tab).
//   Skazi303Shot <out folder>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "shots");
    outDir.createDirectory();

    for (int tab = 0; tab < 4; ++tab)
    {

        Skazi303Processor proc;
        proc.setRateAndBufferSizeDetails (48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        proc.apvts.state.setProperty ("uiTab", tab, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (editor->getWidth() * 4 / 3, editor->getHeight() * 4 / 3); // full size
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto out = outDir.getChildFile ("tbon_tab" + juce::String (tab) + ".png");
        out.deleteFile();
        juce::FileOutputStream stream (out);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    }
    return 0;
}
