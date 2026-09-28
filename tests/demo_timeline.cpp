#include "DemoTimeline.h"
#include <vector>
#include <iostream>

int main() {
    using MixEngine::DemoTimeline;
    constexpr int sampleRate = 1000;

    std::vector<float> left(70000, 1.0f);
    std::vector<float> right(70000, 1.0f);
    float* outputs[2]{left.data(), right.data()};

    DemoTimeline demo;
    demo.configure(sampleRate, false);
    demo.processAndAdvance(outputs, 2, static_cast<int>(left.size()));

    for (int i = 0; i < 60000; ++i)
        if (left[i] != 1.0f || right[i] != 1.0f) return 1;

    if (!(left[60000] < 1.0f && left[60000] > 0.0f)) return 2;
    if (left[60020] != 0.0f || right[60020] != 0.0f) return 3;
    if (left[63000] != 1.0f || right[63000] != 1.0f) return 4;

    std::fill(left.begin(), left.end(), 1.0f);
    std::fill(right.begin(), right.end(), 1.0f);

    DemoTimeline full;
    full.configure(sampleRate, true);
    full.processAndAdvance(outputs, 2, static_cast<int>(left.size()));
    for (std::size_t i = 0; i < left.size(); ++i)
        if (left[i] != 1.0f || right[i] != 1.0f) return 5;

    std::cout << "MixEngine DemoTimeline PASS\n";
    return 0;
}
