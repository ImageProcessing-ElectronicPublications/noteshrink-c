#include "noteshrink.h"

#include <cassert>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <vector>

namespace
{
int const bitsPerSample = 6;
}

static NSHRgb NSHRgbAdd(NSHRgb a, NSHRgb b)
{
    NSHRgb r;
    r.R = a.R + b.R;
    r.G = a.G + b.G;
    r.B = a.B + b.B;
    return r;
}

static NSHRgb NSHRgbMul(NSHRgb a, float scalar)
{
    NSHRgb r;
    r.R = a.R * scalar;
    r.G = a.G * scalar;
    r.B = a.B * scalar;
    return r;
}

static float NSHRgbSquareDistance(NSHRgb a, NSHRgb b)
{
    float squareDistance = 0;
    squareDistance += (a.R - b.R) * (a.R - b.R);
    squareDistance += (a.G - b.G) * (a.G - b.G);
    squareDistance += (a.B - b.B) * (a.B - b.B);
    return squareDistance;
}

static int NSHRgbClosest(NSHRgb p, std::vector<NSHRgb> const& means)
{
    int idx = 0;
    float minimum = NSHRgbSquareDistance(p, means[0]);
    for (size_t i = 0; i < means.size(); i++)
    {
        float squaredDistance = NSHRgbSquareDistance(p, means[i]);
        if (squaredDistance < minimum)
        {
            minimum = squaredDistance;
            idx = i;
        }
    }
    return idx;
}

static void ColorRgbToHsv(NSHRgb p, float& h, float& s, float& v)
{
    float r = p.R / 255.0f;
    float g = p.G / 255.0f;
    float b = p.B / 255.0f;
    float max = std::max(std::max(r, g), b);
    float min = std::min(std::min(r, g), b);
    h = max - min;
    if (h > 0)
    {
        if (max == r)
        {
            h = (g - b) / h;
            if (h < 0)
            {
                h += 6;
            }
        }
        else if (max == g)
        {
            h = 2 + (b - r) / h;
        }
        else
        {
            h = 4 + (r - g) / h;
        }
    }
    h /= 6;
    s = max - min;
    if (max > 0)
    {
        s /= max;
    }
    v = max;
}

static NSHRgb ColorHsvToRgb(float h, float s, float v)
{
    float r = v;
    float g = v;
    float b = v;
    if (s > 0)
    {
        h *= 6.;
        int i = int(h);
        float f = h - float(i);
        switch (i)
        {
        default:
        case 0:
            g *= 1 - s * (1 - f);
            b *= 1 - s;
            break;
        case 1:
            r *= 1 - s * f;
            b *= 1 - s;
            break;
        case 2:
            r *= 1 - s;
            b *= 1 - s * (1 - f);
            break;
        case 3:
            r *= 1 - s;
            g *= 1 - s * f;
            break;
        case 4:
            r *= 1 - s * (1 - f);
            g *= 1 - s;
            break;
        case 5:
            g *= 1 - s;
            b *= 1 - s * f;
            break;
        }
    }
    NSHRgb p;
    p.R = r * 255;
    p.G = g * 255;
    p.B = b * 255;
    return p;
}

static void ImageSamplePixels(NSHRgb* input, size_t inputSize, NSHOption o, std::vector<NSHRgb>& samples)
{
    samples.clear();
    size_t numSamples = (size_t)std::min(std::max(float(inputSize) * o.SampleFraction, float(0)), float(inputSize));
    size_t interval = std::max((size_t)1, inputSize / numSamples);
    for (size_t i = 0; i < inputSize; i += interval)
    {
        samples.push_back(input[i]);
    }
}

static void ImageQuantize(std::vector<NSHRgb> const& image, int bitsPerChannel, std::vector<uint32_t>& quantized)
{
    uint8_t shift = 8 - bitsPerChannel;
    uint8_t halfbin = uint8_t((1 << shift) >> 1);

    quantized.clear();
    quantized.reserve(image.size());

    for (size_t i = 0; i < image.size(); i++)
    {
        uint32_t r = ((uint8_t(image[i].R) >> shift) << shift) + halfbin;
        uint32_t g = ((uint8_t(image[i].G) >> shift) << shift) + halfbin;
        uint32_t b = ((uint8_t(image[i].B) >> shift) << shift) + halfbin;
        uint32_t p = (((r << 8) | g) << 8) | b;
        quantized.push_back(p);
    }
}

static void ImageKMeans(std::vector<NSHRgb> const& data, int k, int maxItr, std::vector<NSHRgb>& means)
{
    means.clear();
    means.reserve(k);

    for (int i = 0; i < k; i++)
    {
        float h = (float(i) + 0.5f) / float(k);
        NSHRgb p = ColorHsvToRgb(h, 1, 1);
        float minval = 196608.0f;
        int k = NSHRgbClosest(p, data);
        means.push_back(data[k]);
    }

    std::vector<int> clusters(data.size());
    for (size_t i = 0; i < data.size(); i++)
    {
        NSHRgb d = data[i];
        clusters[i] = NSHRgbClosest(d, means);
    }

    std::vector<int> mLen(k);
    for (int itr = 0; itr < maxItr; itr++)
    {
        for (size_t i = 0; i < k; i++)
        {
            NSHRgb p;
            p.R = p.G = p.B = 0;
            means[i] = p;
            mLen[i] = 0;
        }
        for (size_t i = 0; i < data.size(); i++)
        {
            NSHRgb p = data[i];
            int cluster = clusters[i];
            NSHRgb m = NSHRgbAdd(means[cluster], p);
            means[cluster] = m;
            mLen[cluster] = mLen[cluster] + 1;
        }
        for (size_t i = 0; i < means.size(); i++)
        {
            int len = std::max(1, mLen[i]);
            NSHRgb m = NSHRgbMul(means[i], 1 / float(len));
            means[i] = m;
        }
        int changes = 0;
        for (size_t i = 0; i < data.size(); i++)
        {
            NSHRgb p = data[i];
            int cluster = NSHRgbClosest(p, means);
            if (cluster != clusters[i])
            {
                changes++;
                clusters[i] = cluster;
            }
        }
        if (changes == 0)
        {
            break;
        }
    }
}

static NSHRgb BackgroundColorFind(std::vector<NSHRgb> const& image, int bitsPerChannel)
{
    std::vector<uint32_t> quantized;
    ImageQuantize(image, bitsPerChannel, quantized);
    std::map<uint32_t, int> count;
    int maxcount = 1;
    uint32_t maxvalue = quantized[0];
    for (size_t i = 1; i < quantized.size(); i++)
    {
        uint32_t v = quantized[i];
        int c = count[v] + 1;
        if (c > maxcount)
        {
            maxcount = c;
            maxvalue = v;
        }
        count[v] = c;
    }

    uint8_t shift = 8 - bitsPerChannel;
    uint8_t r = (maxvalue >> 16) & 0xff;
    uint8_t g = (maxvalue >> 8) & 0xff;
    uint8_t b = maxvalue & 0xff;

    NSHRgb bg;
    bg.R = r;
    bg.G = g;
    bg.B = b;

    return bg;
}

static void ForegroundMaskCreate(NSHRgb bgColor, std::vector<NSHRgb> const& samples, NSHOption option, std::vector<bool>& mask)
{
    float hBg, sBg, vBg;
    ColorRgbToHsv(bgColor, hBg, sBg, vBg);
    std::vector<float> sSamples;
    sSamples.reserve(samples.size());
    std::vector<float> vSamples;
    vSamples.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
    {
        float h, s, v;
        ColorRgbToHsv(samples[i], h, s, v);
        sSamples.push_back(s);
        vSamples.push_back(v);
    }

    mask.clear();
    mask.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
    {
        float sDiff = fabs(sBg - sSamples[i]);
        float vDiff = fabs(vBg - vSamples[i]);
        bool fg = vDiff >= option.BrightnessThreshold || sDiff >= option.SaturationThreshold;
        mask.push_back(fg);
    }
}

static void ForegroundMaskDespeckle(std::vector<bool>& fgMask, NSHOption option, int width, int height)
{
    if (option.Despeckle > 0)
    {
        // Despeckle (2*r+1)x(2*r+1)
        int r = option.Despeckle;
        int a2 = (2 * r + 1) * (2 * r + 1) / 2 + 1;
        int k = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (!fgMask[k])
                {
                    int l = 0;
                    for (int i = -r; i < (r + 1); i++)
                    {
                        int yf = y + i;
                        yf = (yf < 0) ? 0 : ((yf < height) ? yf : (height - 1));
                        for (int j = -r; j < (r + 1); j++)
                        {
                            int xf = x + j;
                            xf = (xf < 0) ? 0 : ((xf < width) ? xf : (width - 1));
                            int kf = width * yf + xf;
                            if (fgMask[k] == fgMask[kf]) l++;
                        }
                    }
                    if (l < a2) fgMask[k] = !fgMask[k];
                }
                k++;
            }
        }
    }
}

static void NSHPaletteGenerate(std::vector<NSHRgb> const& samples, NSHRgb bgColor, NSHOption option, std::vector<NSHRgb>& outPalette)
{
    std::vector<bool> fgMask;
    ForegroundMaskCreate(bgColor, samples, option, fgMask);
    std::vector<NSHRgb> data;

    for (int i = 0; i < samples.size(); i++)
    {
        if (fgMask[i])
        {
            NSHRgb v = samples[i];
            data.push_back(v);
        }
    }

    std::vector<NSHRgb> means;
    ImageKMeans(data, outPalette.size() - 1, option.KmeansMaxIter, means);

    size_t idx = 0;
    outPalette[idx++] = bgColor;
    for (size_t i = 0; i < means.size(); i++)
    {
        NSHRgb c = means[i];
        c.R = round(c.R);
        c.G = round(c.G);
        c.B = round(c.B);
        outPalette[idx++] = c;
    }
}

extern "C" NSHOption NSHMakeDefaultOption()
{
    NSHOption o;
    o.SampleFraction = 0.05f;
    o.BrightnessThreshold = 0.25f;
    o.SaturationThreshold = 0.20f;
    o.KmeansMaxIter = 40;
    o.Saturate = false;
    o.Norm = false;
    o.WhiteBackground = false;
    o.NumColors = 6;
    o.Despeckle = 1;
    return o;
}

extern "C" bool NSHPaletteCreate(std::vector<NSHRgb>& input, size_t inputSize, NSHOption option, std::vector<NSHRgb>& palette)
{
    if (input.empty() || palette.empty())
    {
        return false;
    }
    if (option.NumColors < 2)
    {
        return false;
    }

    std::vector<NSHRgb> samples;
    ImageSamplePixels(input.data(), inputSize, option, samples);
    NSHRgb bgColor = BackgroundColorFind(samples, bitsPerSample);
    NSHPaletteGenerate(samples, bgColor, option, palette);

    return true;
}

extern "C" bool NSHPaletteApply(std::vector<NSHRgb>& img, std::vector<NSHRgb>& palette, int width, int height, NSHOption option, std::vector<uint8_t>& result)
{
    std::vector<bool> fgMask;
    NSHRgb bgColor = palette[0];
    ForegroundMaskCreate(bgColor, img, option, fgMask);
    ForegroundMaskDespeckle(fgMask, option, width, height);
    for (int i = 0; i < img.size(); i++)
    {
        if (!fgMask[i])
        {
            result.push_back(0);
        }
        else
        {
            NSHRgb p = img[i];
            int minIdx = NSHRgbClosest(p, palette);
            result.push_back(minIdx);
        }
    }

    return true;
}

extern "C" bool NSHPaletteSaturate(std::vector<NSHRgb>& palette)
{
    float maxSat = 0.0f, minSat = 1.0f;

    for (int i = 0; i < palette.size(); i++)
    {
        float h, s, v;
        ColorRgbToHsv(palette[i], h, s, v);
        maxSat = std::max(maxSat, s);
        minSat = std::min(minSat, s);
    }
    if (maxSat > minSat)
    {
        for (int i = 0; i < palette.size(); i++)
        {
            float h, s, v;
            ColorRgbToHsv(palette[i], h, s, v);
            float newSat = (s - minSat) / (maxSat - minSat);
            palette[i] = ColorHsvToRgb(h, newSat, v);
        }
    }

    return true;
}

extern "C" bool NSHPaletteNorm(std::vector<NSHRgb>& palette)
{
    float maxVal = 0.0f, minVal = 1.0f;

    for (int i = 0; i < palette.size(); i++)
    {
        float h, s, v;
        ColorRgbToHsv(palette[i], h, s, v);
        maxVal = std::max(maxVal, v);
        minVal = std::min(minVal, v);
    }
    if (maxVal > minVal)
    {
        for (int i = 0; i < palette.size(); i++)
        {
            float h, s, v;
            ColorRgbToHsv(palette[i], h, s, v);
            float newVal = (v - minVal) / (maxVal - minVal);
            palette[i] = ColorHsvToRgb(h, s, newVal);
        }
    }

    return true;
}
