#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdbool>
#include <vector>

#ifndef __NOTESHRINK_H
#define __NOTESHRINK_H
#define NOTESHRINK_VERSION "1.4"

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
    int NumColors;
    int Despeckle;
    bool Saturate;
    bool Norm;
    bool WhiteBackground;
} NSHOption;

extern NSHOption NSHMakeDefaultOption();

extern bool NSHPaletteCreate(std::vector<NSHRgb>& input, size_t inputSize, NSHOption option, std::vector<NSHRgb>& palette);

extern bool NSHPaletteApply(std::vector<NSHRgb>& img, std::vector<NSHRgb>& palette, int width, int height, NSHOption option, std::vector<uint8_t>& result);

extern bool NSHPaletteSaturate(std::vector<NSHRgb>& palette);

extern bool NSHPaletteNorm(std::vector<NSHRgb>& palette);

#ifdef __cplusplus
} // extern "C"
#endif // _\cplusplus

#endif /* __NOTESHRINK_H */
