#include "audio_io.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace mvtest {

std::vector<std::vector<float>> readWav(const std::string& path, double& outSampleRate) {
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    juce::File f(path);
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
    if (!reader) return {};
    outSampleRate = reader->sampleRate;
    int nCh = (int) reader->numChannels;
    int n   = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf(nCh, n);
    reader->read(&buf, 0, n, 0, true, true);
    std::vector<std::vector<float>> ch(nCh, std::vector<float>(n));
    for (int c = 0; c < nCh; ++c)
        std::copy(buf.getReadPointer(c), buf.getReadPointer(c) + n, ch[c].begin());
    return ch;
}

void writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, double sampleRate) {
    if (channels.empty()) return;
    juce::WavAudioFormat fmt;
    juce::File f(path);
    f.deleteFile();
    auto* stream = f.createOutputStream().release();
    std::unique_ptr<juce::AudioFormatWriter> writer(
        fmt.createWriterFor(stream, sampleRate, (unsigned) channels.size(), 16, {}, 0));
    if (!writer) { delete stream; return; }
    const int n = (int) channels[0].size();
    juce::AudioBuffer<float> buf((int) channels.size(), n);
    for (int c = 0; c < (int) channels.size(); ++c)
        std::copy(channels[c].begin(), channels[c].end(), buf.getWritePointer(c));
    writer->writeFromAudioSampleBuffer(buf, 0, n);
}

}
