#include "PluginEditor.h"

MultivoicerEditor::MultivoicerEditor(MultivoicerProcessor& p)
    : juce::AudioProcessorEditor(p), generic(p) {
    addAndMakeVisible(generic);
    setSize(640, 800);
    setResizable(true, true);
}

void MultivoicerEditor::resized() {
    generic.setBounds(getLocalBounds());
}
