#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdbool>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    float R; // [0, 255]
    float G;
    float B;
} NSHRgb;


typedef struct {
    float SampleFraction;
    float BrightnessThreshold;
    float SaturationThreshold;
    int KmeansMaxIter;
    int NumColors = 6;
    bool Saturate;
    bool WhiteBackground;
} NSHOption;


extern NSHOption NSHMakeDefaultOption();

extern bool NSHCreatePalette(std::vector<NSHRgb>& input, size_t inputSize, NSHOption option, std::vector<NSHRgb>& palette, std::vector<uint8_t>& result, int width, int height);

#ifdef __cplusplus
} // extern "C"
#endif // _\cplusplus
