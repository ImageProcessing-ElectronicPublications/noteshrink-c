#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef __NOTESHRINK_H
#define __NOTESHRINK_H
#define NOTESHRINK_VERSION "2.2"

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

extern bool NSHPaletteCreate(NSHRgb* input, size_t inputSize, NSHOption option, NSHRgb* palette, size_t paletteSize);

extern bool NSHPaletteApply(NSHRgb* img, size_t imgSize, NSHRgb* palette, size_t paletteSize, int width, int height, NSHOption option, uint8_t *result);

extern bool NSHPaletteSaturate(NSHRgb *palette, size_t paletteSize);

extern bool NSHPaletteNorm(NSHRgb *palette, size_t paletteSize);

#ifdef __cplusplus
} // extern "C"
#endif // _\cplusplus

#endif /* __NOTESHRINK_H */
