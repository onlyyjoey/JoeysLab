// JoeysLab plugin shell: hosts the JoeysLab web app inside a VST3 / AU window.
// Phase 1: the app's own audio engine plays through the Mac's output; exports save to ~/Music/JoeysLab Exports.
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
juce::File webRoot()
{
    auto bundle = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    auto dir = bundle.getChildFile ("Contents/Resources/web");
    if (! dir.isDirectory())
        dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                  .getParentDirectory().getParentDirectory().getChildFile ("Resources/web");
    return dir;
}

juce::String mimeFor (const juce::String& ext)
{
    static const std::map<juce::String, juce::String> m {
        { "html", "text/html" }, { "js", "text/javascript" }, { "css", "text/css" },
        { "json", "application/json" }, { "svg", "image/svg+xml" }, { "png", "image/png" },
        { "wav", "audio/wav" }, { "mp3", "audio/mpeg" }, { "webmanifest", "application/manifest+json" } };
    auto it = m.find (ext.toLowerCase());
    return it != m.end() ? it->second : juce::String ("application/octet-stream");
}
}

class JoeysLabProcessor : public juce::AudioProcessor
{
public:
    JoeysLabProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "JoeysLab"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override { buffer.clear(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JoeysLabProcessor)
};

class JoeysLabEditor : public juce::AudioProcessorEditor
{
public:
    explicit JoeysLabEditor (JoeysLabProcessor& p)
        : AudioProcessorEditor (p),
          web (juce::WebBrowserComponent::Options{}
                   .withNativeIntegrationEnabled()
                   .withKeepPageLoadedWhenBrowserIsHidden()
                   .withResourceProvider ([] (const juce::String& url) { return serve (url); })
                   .withEventListener ("joeyslab_save", [] (juce::var v) { save (v); }))
    {
        addAndMakeVisible (web);
        setResizable (true, true);
        setResizeLimits (820, 520, 2600, 1700);
        setSize (1280, 820);
        web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    }

    void resized() override { web.setBounds (getLocalBounds()); }

private:
    static std::optional<juce::WebBrowserComponent::Resource> serve (const juce::String& url)
    {
        auto path = url.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
        if (path.isEmpty())
            path = "index.html";
        auto root = webRoot();
        auto f = root.getChildFile (path);
        if (! f.existsAsFile() || ! f.isAChildOf (root))
            return std::nullopt;
        juce::MemoryBlock mb;
        if (! f.loadFileAsData (mb))
            return std::nullopt;
        std::vector<std::byte> data (mb.getSize());
        std::memcpy (data.data(), mb.getData(), mb.getSize());
        return juce::WebBrowserComponent::Resource { std::move (data), mimeFor (f.getFileExtension().substring (1)) };
    }

    static void save (const juce::var& v)
    {
        auto name = juce::File::createLegalFileName (v.getProperty ("name", "JoeysLab export").toString());
        juce::MemoryOutputStream out;
        if (! juce::Base64::convertFromBase64 (out, v.getProperty ("data", "").toString()))
            return;
        auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("JoeysLab Exports");
        dir.createDirectory();
        auto target = dir.getNonexistentChildFile (juce::File (name).getFileNameWithoutExtension(),
                                                   juce::File (name).getFileExtension(), false);
        if (target.replaceWithData (out.getData(), out.getDataSize()))
            target.revealToUser();
    }

    juce::WebBrowserComponent web;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JoeysLabEditor)
};

juce::AudioProcessorEditor* JoeysLabProcessor::createEditor() { return new JoeysLabEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JoeysLabProcessor(); }
