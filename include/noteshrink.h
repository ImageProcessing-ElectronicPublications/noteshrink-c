#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifndef __NOTESHRINK_H
#define __NOTESHRINK_H
#define NOTESHRINK_VERSION "2.3"

#ifdef NOTESHRINK_STATIC
#define NOTESHRINKAPI static
#else
#define NOTESHRINKAPI extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

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

NOTESHRINKAPI NSHOption NSHMakeDefaultOption();
NOTESHRINKAPI bool NSHPaletteCreate(NSHRgb* input, size_t inputSize, NSHOption option, NSHRgb* palette, size_t paletteSize);
NOTESHRINKAPI bool NSHPaletteApply(NSHRgb* img, size_t imgSize, NSHRgb* palette, size_t paletteSize, int width, int height, NSHOption option, uint8_t *result);
NOTESHRINKAPI bool NSHPaletteSaturate(NSHRgb *palette, size_t paletteSize);
NOTESHRINKAPI bool NSHPaletteNorm(NSHRgb *palette, size_t paletteSize);

#ifdef __cplusplus
}
#endif

#endif /* __NOTESHRINK_H */
