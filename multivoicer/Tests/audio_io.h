#pragma once
#include <string>
#include <vector>

namespace mvtest {
std::vector<std::vector<float>> readWav(const std::string& path, double& outSampleRate);
void writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, double sampleRate);
}
