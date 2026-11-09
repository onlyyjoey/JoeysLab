// JoeysLab plugin: hosts the JoeysLab web app, follows the host's tempo and transport,
// sends each stem out as MIDI on its own channel, and lets you drag stems into the DAW as .mid files.
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

const juce::Colour kBg { 0xff0b0d12 }, kPanel { 0xff141821 }, kLine { 0xff262c38 },
                   kText { 0xffe6e9ef }, kDim { 0xff8a93a6 }, kAccent { 0xff3d8bff };
}

//==============================================================================
// Song data sent from the web app. Steps are 16th notes.
struct SongNote  { int pitch = 60; double start = 0, length = 1; float velocity = 0.8f; };
struct SongTrack { juce::String name; int channel = 1; bool enabled = true; std::vector<SongNote> notes; };
struct Song      { double totalSteps = 128; double bpm = 120; std::vector<SongTrack> tracks; };

//==============================================================================
class JoeysLabProcessor : public juce::AudioProcessor
{
public:
    JoeysLabProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "JoeysLab"; }
    void prepareToPlay (double sr, int) override { sampleRate = sr; }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    // ---- song exchange (message thread -> audio thread)
    void setSong (std::shared_ptr<const Song> s)
    {
        { const juce::SpinLock::ScopedLockType l (songLock); pendingSong = std::move (s); }
        songVersion++;
    }
    std::shared_ptr<const Song> getSongCopy() const
    {
        const juce::SpinLock::ScopedLockType l (songLock);
        return pendingSong;
    }

    std::atomic<bool> hostPlaying { false }, midiOut { true }, followHost { true }, pluginSounds { true };
    std::atomic<double> hostPpq { 0 }, hostBpm { 120 };
    std::atomic<int> songVersion { 0 };

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        buffer.clear();
        midi.clear();
        const int n = buffer.getNumSamples();

        {   // pick up a new song if one is waiting (never blocks the audio thread)
            const juce::SpinLock::ScopedTryLockType l (songLock);
            if (l.isLocked() && pendingSong != song)
            {
                allNotesOff (midi, 0);
                song = pendingSong;
            }
        }

        bool playing = false; double ppq = 0, bpm = 120;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                playing = pos->getIsPlaying();
                ppq = pos->getPpqPosition().orFallback (0.0);
                bpm = pos->getBpm().orFallback (120.0);
            }
        hostPlaying = playing; hostPpq = ppq; hostBpm = bpm;

        if (! playing || ! midiOut.load() || song == nullptr || song->totalSteps <= 0)
        {
            if (wasPlaying || ! active.empty()) allNotesOff (midi, 0);
            wasPlaying = false;
            return;
        }

        const double stepsPerSample = bpm / 60.0 * 4.0 / sampleRate;
        const double a = ppq * 4.0, b = a + n * stepsPerSample;
        if (! wasPlaying || std::abs (a - lastEnd) > 0.05)   // started or jumped
            allNotesOff (midi, 0);
        wasPlaying = true;
        lastEnd = b;

        auto offset = [&] (double step) { return juce::jlimit (0, n - 1, (int) ((step - a) / stepsPerSample)); };

        // note-offs due in this block
        for (auto it = active.begin(); it != active.end();)
        {
            if (it->off < b) { midi.addEvent (juce::MidiMessage::noteOff (it->ch, it->pitch), offset (it->off)); it = active.erase (it); }
            else ++it;
        }

        // note-ons, looping over the song length
        const double T = song->totalSteps;
        const double k0 = std::floor (a / T), k1 = std::floor (b / T);
        for (auto& t : song->tracks)
        {
            if (! t.enabled) continue;
            for (auto& note : t.notes)
                for (double k = k0; k <= k1; k += 1.0)
                {
                    const double on = k * T + note.start;
                    if (on < a || on >= b) continue;
                    for (auto it = active.begin(); it != active.end();)   // retrigger: close the old one first
                        if (it->ch == t.channel && it->pitch == note.pitch) { midi.addEvent (juce::MidiMessage::noteOff (it->ch, it->pitch), offset (on)); it = active.erase (it); }
                        else ++it;
                    midi.addEvent (juce::MidiMessage::noteOn (t.channel, note.pitch, juce::jlimit (0.05f, 1.0f, note.velocity)), offset (on));
                    const double off = on + juce::jmax (0.05, note.length);
                    if (off < b) midi.addEvent (juce::MidiMessage::noteOff (t.channel, note.pitch), offset (off));
                    else active.push_back ({ t.channel, note.pitch, off });
                }
        }
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        juce::ValueTree v ("JoeysLab");
        v.setProperty ("midiOut", midiOut.load(), nullptr);
        v.setProperty ("follow", followHost.load(), nullptr);
        v.setProperty ("sounds", pluginSounds.load(), nullptr);
        v.setProperty ("songJson", lastSongJson, nullptr);
        juce::MemoryOutputStream out (dest, false);
        v.writeToStream (out);
    }
    void setStateInformation (const void* data, int size) override
    {
        auto v = juce::ValueTree::readFromData (data, (size_t) size);
        if (! v.isValid()) return;
        midiOut = (bool) v.getProperty ("midiOut", true);
        followHost = (bool) v.getProperty ("follow", true);
        pluginSounds = (bool) v.getProperty ("sounds", true);
        lastSongJson = v.getProperty ("songJson", "").toString();
    }

    juce::String lastSongJson;   // the web app's song, saved with the DAW project

private:
    struct ActiveNote { int ch, pitch; double off; };

    void allNotesOff (juce::MidiBuffer& midi, int pos)
    {
        for (auto& an : active) midi.addEvent (juce::MidiMessage::noteOff (an.ch, an.pitch), pos);
        active.clear();
        for (int ch = 1; ch <= 16; ++ch) midi.addEvent (juce::MidiMessage::allNotesOff (ch), pos);
    }

    double sampleRate = 44100, lastEnd = 0;
    bool wasPlaying = false;
    std::vector<ActiveNote> active;
    std::shared_ptr<const Song> song, pendingSong;
    mutable juce::SpinLock songLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JoeysLabProcessor)
};

//==============================================================================
// Writes stems to a .mid file in a temp folder, for dragging into the DAW.
static juce::File writeMidi (const Song& song, int trackIndex)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (960);
    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (1.0, song.bpm))), 0);
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0);
    mf.addTrack (tempo);

    juce::String name = "JoeysLab";
    for (int i = 0; i < (int) song.tracks.size(); ++i)
    {
        if (trackIndex >= 0 && i != trackIndex) continue;
        auto& t = song.tracks[(size_t) i];
        juce::MidiMessageSequence seq;
        auto title = juce::MidiMessage::textMetaEvent (3, t.name);
        seq.addEvent (title, 0);
        for (auto& note : t.notes)
        {
            const double on = note.start * 240.0, off = (note.start + note.length) * 240.0;
            seq.addEvent (juce::MidiMessage::noteOn (t.channel, note.pitch, juce::jlimit (0.05f, 1.0f, note.velocity)), on);
            seq.addEvent (juce::MidiMessage::noteOff (t.channel, note.pitch), off);
        }
        seq.updateMatchedPairs();
        mf.addTrack (seq);
        if (trackIndex >= 0) name = t.name;
    }
    if (trackIndex < 0) name = "JoeysLab all stems";

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("JoeysLab MIDI");
    dir.createDirectory();
    auto f = dir.getChildFile (juce::File::createLegalFileName (name) + ".mid");
    f.deleteFile();
    if (auto os = f.createOutputStream()) mf.writeTo (*os);
    return f;
}

//==============================================================================
class DragChip : public juce::Component
{
public:
    DragChip (JoeysLabProcessor& p, int idx, juce::String label, juce::Colour c)
        : proc (p), index (idx), text (std::move (label)), colour (c)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
    int idealWidth() const { return juce::GlyphArrangement::getStringWidthInt (juce::FontOptions (12.0f, juce::Font::bold), text) + 30; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (isMouseOver() ? kLine.brighter (0.2f) : kLine);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (r.removeFromLeft (4.0f).reduced (0, 4), 2.0f);
        g.setColour (kText);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\xa0\xbf ")) + text, getLocalBounds().withTrimmedLeft (10), juce::Justification::centredLeft);
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 4) return;
        auto s = proc.getSongCopy();
        if (s == nullptr) return;
        dragging = true;
        auto f = writeMidi (*s, index);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                   [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

private:
    JoeysLabProcessor& proc;
    int index;
    juce::String text;
    juce::Colour colour;
    bool dragging = false;
};

//==============================================================================
class JoeysLabEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JoeysLabEditor (JoeysLabProcessor& p)
        : AudioProcessorEditor (p), proc (p),
          web (juce::WebBrowserComponent::Options{}
                   .withNativeIntegrationEnabled()
                   .withKeepPageLoadedWhenBrowserIsHidden()
                   .withResourceProvider ([] (const juce::String& url) { return serve (url); })
                   .withEventListener ("joeyslab_save", [] (juce::var v) { save (v); })
                   .withEventListener ("joeyslab_song", [this] (juce::var v) { receiveSong (v); })
                   .withEventListener ("joeyslab_ready", [this] (juce::var) { sendHost (true); restoreSong(); }))
    {
        addAndMakeVisible (web);
        for (auto* b : { &followBtn, &soundsBtn, &midiBtn })
        {
            addAndMakeVisible (*b);
            b->setClickingTogglesState (true);
            b->setColour (juce::TextButton::buttonColourId, kLine);
            b->setColour (juce::TextButton::buttonOnColourId, kAccent);
            b->setColour (juce::TextButton::textColourOffId, kDim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }
        followBtn.setToggleState (proc.followHost, juce::dontSendNotification);
        soundsBtn.setToggleState (proc.pluginSounds, juce::dontSendNotification);
        midiBtn.setToggleState (proc.midiOut, juce::dontSendNotification);
        followBtn.setTooltip ("JoeysLab starts, stops and changes tempo with FL Studio");
        soundsBtn.setTooltip ("Play JoeysLab's own sounds. Turn off to hear only the synths you route the MIDI to");
        midiBtn.setTooltip ("Send each stem out as MIDI (drums on channel 10, other stems on 1, 2, 3...)");
        followBtn.onClick = [this] { proc.followHost = followBtn.getToggleState(); sendHost (true); };
        soundsBtn.onClick = [this] { proc.pluginSounds = soundsBtn.getToggleState(); sendHost (true); };
        midiBtn.onClick   = [this] { proc.midiOut = midiBtn.getToggleState(); };

        addAndMakeVisible (status);
        status.setColour (juce::Label::textColourId, kDim);
        status.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (dragLabel);
        dragLabel.setText ("Drag MIDI:", juce::dontSendNotification);
        dragLabel.setColour (juce::Label::textColourId, kDim);
        dragLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        setResizable (true, true);
        setResizeLimits (820, 560, 2600, 1700);
        setSize (1280, 860);
        web.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
        startTimerHz (30);
    }
    ~JoeysLabEditor() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBg);
        auto bar = getLocalBounds().removeFromBottom (barH);
        g.setColour (kPanel);
        g.fillRect (bar);
        g.setColour (kLine);
        g.fillRect (bar.removeFromTop (1));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromBottom (barH).reduced (8, 7);
        web.setBounds (r);
        followBtn.setBounds (bar.removeFromLeft (92)); bar.removeFromLeft (6);
        soundsBtn.setBounds (bar.removeFromLeft (98)); bar.removeFromLeft (6);
        midiBtn.setBounds (bar.removeFromLeft (78));   bar.removeFromLeft (10);
        status.setBounds (bar.removeFromLeft (150));   bar.removeFromLeft (6);
        dragLabel.setBounds (bar.removeFromLeft (70));
        for (auto& c : chips)
        {
            const int w = juce::jmin (c->idealWidth(), bar.getWidth());
            if (w < 40) { c->setVisible (false); continue; }
            c->setVisible (true);
            c->setBounds (bar.removeFromLeft (w));
            bar.removeFromLeft (5);
        }
    }

private:
    static constexpr int barH = 42;

    void timerCallback() override
    {
        const bool playing = proc.hostPlaying;
        const double bpm = proc.hostBpm;
        status.setText ((playing ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 FL playing  ")) : juce::String ("FL stopped  "))
                          + juce::String (bpm, 1) + " BPM", juce::dontSendNotification);
        sendHost (false);
        if (seenVersion != proc.songVersion.load()) { seenVersion = proc.songVersion; rebuildChips(); }
    }

    // Tell the web app about the host transport: on play/stop changes, tempo changes, and setting changes.
    void sendHost (bool force)
    {
        const bool playing = proc.hostPlaying;
        const double bpm = proc.hostBpm;
        if (! force && playing == lastPlaying && std::abs (bpm - lastBpm) < 0.01) return;
        lastPlaying = playing; lastBpm = bpm;
        auto* o = new juce::DynamicObject();
        o->setProperty ("playing", playing);
        o->setProperty ("ppq", proc.hostPpq.load());
        o->setProperty ("bpm", bpm);
        o->setProperty ("follow", proc.followHost.load());
        o->setProperty ("sounds", proc.pluginSounds.load());
        web.emitEventIfBrowserIsVisible ("joeyslab_host", juce::var (o));
    }

    void receiveSong (const juce::var& v)
    {
        auto s = std::make_shared<Song>();
        s->totalSteps = (double) v.getProperty ("total", 128.0);
        s->bpm = (double) v.getProperty ("bpm", 120.0);
        if (auto* arr = v.getProperty ("tracks", {}).getArray())
            for (auto& tv : *arr)
            {
                SongTrack t;
                t.name = tv.getProperty ("name", "Stem").toString();
                t.channel = juce::jlimit (1, 16, (int) tv.getProperty ("ch", 1));
                t.enabled = (bool) tv.getProperty ("on", true);
                if (auto* notes = tv.getProperty ("notes", {}).getArray())
                    for (auto& nv : *notes)
                        if (auto* a = nv.getArray(); a != nullptr && a->size() >= 4)
                            t.notes.push_back ({ juce::jlimit (0, 127, (int) (*a)[0]), (double) (*a)[1], (double) (*a)[2], (float) (double) (*a)[3] });
                s->tracks.push_back (std::move (t));
            }
        proc.lastSongJson = juce::JSON::toString (v, true);
        proc.setSong (s);
    }

    void restoreSong()
    {
        if (proc.lastSongJson.isEmpty()) return;
        auto v = juce::JSON::parse (proc.lastSongJson);
        if (v.isObject()) { receiveSong (v); web.emitEventIfBrowserIsVisible ("joeyslab_restore", v); }
    }

    void rebuildChips()
    {
        chips.clear();
        auto s = proc.getSongCopy();
        if (s == nullptr) return;
        static const juce::Colour pal[] { juce::Colour (0xff3d8bff), juce::Colour (0xff35c2e0), juce::Colour (0xffa78bfa),
                                          juce::Colour (0xfff472b6), juce::Colour (0xff34d399), juce::Colour (0xfffbbf24) };
        if (s->tracks.size() > 1)
            chips.push_back (std::make_unique<DragChip> (proc, -1, "All", kText));
        for (int i = 0; i < (int) s->tracks.size(); ++i)
            chips.push_back (std::make_unique<DragChip> (proc, i, s->tracks[(size_t) i].name, pal[i % 6]));
        for (auto& c : chips) addAndMakeVisible (*c);
        resized();
    }

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

    JoeysLabProcessor& proc;
    juce::WebBrowserComponent web;
    juce::TextButton followBtn { "Follow FL" }, soundsBtn { "Plugin sounds" }, midiBtn { "MIDI out" };
    juce::Label status, dragLabel;
    std::vector<std::unique_ptr<DragChip>> chips;
    int seenVersion = -1;
    bool lastPlaying = false;
    double lastBpm = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JoeysLabEditor)
};

juce::AudioProcessorEditor* JoeysLabProcessor::createEditor() { return new JoeysLabEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JoeysLabProcessor(); }
