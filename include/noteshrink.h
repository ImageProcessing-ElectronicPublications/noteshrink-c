#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#ifndef __NOTESHRINK_H
#define __NOTESHRINK_H
#define NOTESHRINK_VERSION "2.6"

#ifdef NOTESHRINK_STATIC
#define NOTESHRINKAPI static
#else
#define NOTESHRINKAPI extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
NOTESHRINKAPI bool NSHPaletteCreate(unsigned char *img, int height, int width, int channels, NSHOption option, float *palette, int paletteSize);
NOTESHRINKAPI bool NSHPaletteApply(unsigned char *img, int height, int width, int channels, float *palette, int paletteSize, NSHOption option, unsigned char *result);
NOTESHRINKAPI bool NSHPaletteSaturate(float *palette, int paletteSize, int channels);
NOTESHRINKAPI bool NSHPaletteNorm(float *palette, int paletteSize, int channels);

#ifdef __cplusplus
}
#endif

#endif /* __NOTESHRINK_H */
