#include "noteshrink.h"

#define bitsPerSample 6

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
    float squareDistance = 0.0f;
    squareDistance += (a.R - b.R) * (a.R - b.R);
    squareDistance += (a.G - b.G) * (a.G - b.G);
    squareDistance += (a.B - b.B) * (a.B - b.B);

    return squareDistance;
}

static size_t NSHRgbClosest(NSHRgb p, NSHRgb* means, size_t meansSize)
{
    size_t idx = 0;
    float minimum = NSHRgbSquareDistance(p, means[0]);
    for (size_t i = 1; i < meansSize; i++)
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

static NSHRgb ColorRgbToHsv(NSHRgb p)
{
    float r = p.R / 255.0f;
    float g = p.G / 255.0f;
    float b = p.B / 255.0f;
    float h, s, v;
    float max = 0.0f, min = 0.0f;
    int imi = 0, ima = 0;
    max = r;
    ima = 1;
    if (max < g)
    {
        max = g;
        ima = 2;
    }
    if (max < b)
    {
        max = b;
        ima = 3;

    }
    min = r;
    imi = 1;
    if (min > g)
    {
        min = g;
        imi = 2;
    }
    if (min > b)
    {
        min = b;
        imi = 3;

    }
    h = max - min;
    if (h > 0.0f)
    {
        if (ima == 1)
        {
            h = (g - b) / h;
            if (h < 0.0f)
            {
                h += 6.0f;
            }
        }
        else if (ima == 2)
        {
            h = 2.0f + (b - r) / h;
        }
        else
        {
            h = 4.0f + (r - g) / h;
        }
    }
    h /= 6.0f;
    s = max - min;
    if (max > 0.0f)
    {
        s /= max;
    }
    v = max;
    NSHRgb hsv;
    hsv.R = h;
    hsv.G = s;
    hsv.B = v;

    return hsv;
}

static NSHRgb ColorHsvToRgb(NSHRgb hsv)
{
    float h = hsv.R;
    float s = hsv.G;
    float v = hsv.B;
    float r = v;
    float g = v;
    float b = v;
    if (s > 0.0f)
    {
        h *= 6.0f;
        int i = (int)h;
        float f = h - (float)i;
        switch (i)
        {
            default:
            case 0:
                g *= 1.0f - s * (1.0f - f);
                b *= 1.0f - s;
                break;
            case 1:
                r *= 1.0f - s * f;
                b *= 1.0f - s;
                break;
            case 2:
                r *= 1.0f - s;
                b *= 1.0f - s * (1.0f - f);
                break;
            case 3:
                r *= 1.0f - s;
                g *= 1.0f - s * f;
                break;
            case 4:
                r *= 1.0f - s * (1.0f - f);
                g *= 1.0f - s;
                break;
            case 5:
                g *= 1.0f - s;
                b *= 1.0f - s * f;
                break;
        }
    }
    NSHRgb p;
    p.R = r * 255.0f;
    p.G = g * 255.0f;
    p.B = b * 255.0f;

    return p;
}

static size_t NSHHSVSearch(NSHRgb p, NSHRgb* means, size_t meansSize)
{
    size_t idx = 0;
    NSHRgb hsv;
    float dH, dS, kS, kV;
    p = ColorRgbToHsv(p);
    float minimum = 5.0f;
    for (size_t i = 0; i < meansSize; i++)
    {
        hsv = ColorRgbToHsv(means[i]);
        dH = (p.R > hsv.R) ? (p.R - hsv.R) : (hsv.R - p.R);
        dS = (p.G > hsv.G) ? (p.G - hsv.G) : (hsv.G - p.G);
        kS = 1.0f + dS;
        kV = (p.B + 0.5f) / (hsv.B + 0.5f);
        if (dH > 0.5f) dH = 1.0f - dH;
        dH *= kS;
        dH *= kV;
        if (dH < minimum)
        {
            minimum = dH;
            idx = i;
        }
    }

    return idx;
}

static size_t ImageSamplePixels(NSHRgb* input, size_t inputSize, NSHRgb* samples, size_t samplesSize, NSHOption o)
{
    size_t k = 0;
    if ((samplesSize > 0) && (samplesSize < inputSize))
    {
        size_t interval = (size_t)inputSize / samplesSize;
        interval = (interval > 0) ? interval : 1;
        for (size_t i = 0; i < inputSize; i += interval)
        {
            if (k < samplesSize)
            {
                samples[k] = input[i];
                k++;
            }
        }
    }
    return k;
}

static void ImageQuantize(NSHRgb* image, size_t imageSize, int bitsPerChannel, uint32_t* quantized)
{
    uint8_t shift = 8 - bitsPerChannel;
    uint8_t halfbin = (((uint8_t)1 << shift) >> 1);

    for (size_t i = 0; i < imageSize; i++)
    {
        uint32_t r = (((uint8_t)image[i].R >> shift) << shift) + halfbin;
        uint32_t g = (((uint8_t)image[i].G >> shift) << shift) + halfbin;
        uint32_t b = (((uint8_t)image[i].B >> shift) << shift) + halfbin;
        uint32_t p = (((r << 8) | g) << 8) | b;
        quantized[i] = p;
    }
}

static bool ImageKMeans(NSHRgb* data, size_t dataSize, NSHRgb* means, int k, int maxItr)
{
    for (int i = 0; i < k; i++)
    {
        float h = ((float)i + 0.5f) / (float)k;
        NSHRgb p;
        p.R = h;
        p.G = 1.0f;
        p.B = 1.0f;
        p = ColorHsvToRgb(p);
        size_t l = NSHRgbClosest(p, data, dataSize);
        p = data[l];
        means[i] = p;
    }

    int* clusters = NULL;
    if (!(clusters = (int*)malloc(dataSize * sizeof(int))))
    {
        return false;
    }
    for (size_t i = 0; i < dataSize; i++)
    {
        NSHRgb d = data[i];
        clusters[i] = NSHRgbClosest(d, means, k);
    }

    int* mLen = NULL;
    if (!(mLen = (int*)malloc(k * sizeof(int))))
    {
        return false;
    }
    for (int itr = 0; itr < maxItr; itr++)
    {
        NSHRgb pm;
        for (int i = 0; i < k; i++)
        {
            NSHRgb p;
            p.R = p.G = p.B = 0;
            means[i] = p;
            mLen[i] = 0;
        }
        for (size_t i = 0; i < dataSize; i++)
        {
            NSHRgb p = data[i];
            int cluster = clusters[i];
            NSHRgb m = NSHRgbAdd(means[cluster], p);
            means[cluster] = m;
            mLen[cluster] = mLen[cluster] + 1;
        }
        int changes = 0;
        for (int i = 0; i < k; i++)
        {
            if (mLen[i] > 0)
            {
                NSHRgb m = NSHRgbMul(means[i], 1.0f / (float)mLen[i]);
                means[i] = m;
            }
            else
            {
                NSHRgb pm, p;
                float h = ((float)i + 0.5f) / (float)k;
                p.R = h;
                p.G = 1.0f;
                p.B = 1.0f;
                p = ColorHsvToRgb(p);
                size_t l = NSHRgbClosest(p, means, k);
                float fk = 1.0f / (float)k;
                pm = NSHRgbMul(means[l], 1.0f - fk);
                p = NSHRgbMul(p, fk);
                p = NSHRgbAdd(p, pm);
                l = NSHRgbClosest(p, data, dataSize);
                p = data[l];
                means[i] = p;
                changes++;
            }
        }
        for (size_t i = 0; i < dataSize; i++)
        {
            NSHRgb p = data[i];
            int cluster = NSHRgbClosest(p, means, k);
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
    free(clusters);
    free(mLen);
    return true;
}

static NSHRgb BackgroundColorFind(NSHRgb* image, size_t imageSize, int bitsPerChannel)
{
    NSHRgb bg;
    uint32_t* quantized = NULL;
    if (!(quantized = (uint32_t*)malloc(imageSize * sizeof(uint32_t))))
    {
        return bg;
    }
    ImageQuantize(image, imageSize, bitsPerChannel, quantized);
    int* count = NULL;
    if (!(count = (int*)malloc(imageSize * sizeof(int))))
    {
        return bg;
    }
    for (size_t i = 0; i < imageSize; i++)
    {
        count[i] = 1;
    }
    for (size_t i = 0; i < imageSize - 1; i++)
    {
        if (count[i] > 0)
        {
            for (size_t j = i + 1; j < imageSize; j++)
            {
                if (quantized[i] == quantized[j])
                {
                    count[i] += count[j];
                    count[j] = 0;
                }
            }
        }
    }
    int maxcount = count[0];
    uint32_t maxvalue = quantized[0];
    for (size_t i = 1; i < imageSize; i++)
    {
        if (maxcount < count[i])
        {
            maxcount = count[i];
            maxvalue = quantized[i];
        }
    }

    uint8_t shift = 8 - bitsPerChannel;
    uint8_t r = (maxvalue >> 16) & 0xff;
    uint8_t g = (maxvalue >> 8) & 0xff;
    uint8_t b = maxvalue & 0xff;

    bg.R = r;
    bg.G = g;
    bg.B = b;

    free(quantized);
    free(count);

    return bg;
}

static void ForegroundMaskCreate(NSHRgb bgColor, NSHRgb* samples, size_t samplesSize, NSHOption option, bool* mask)
{
    NSHRgb bghsv = ColorRgbToHsv(bgColor);
    for (size_t i = 0; i < samplesSize; i++)
    {
        NSHRgb hsv = ColorRgbToHsv(samples[i]);
        float sd = (bghsv.G > hsv.G) ? (bghsv.G - hsv.G) : (hsv.G - bghsv.G);
        float vd = (bghsv.B > hsv.B) ? (bghsv.B - hsv.B) : (hsv.B - bghsv.B);
        mask[i] = vd >= option.BrightnessThreshold || sd >= option.SaturationThreshold;
    }
}

static void ForegroundMaskDespeckle(bool* fgMask, int width, int height, NSHOption option)
{
    if (option.Despeckle > 0)
    {
        // Despeckle (2*r+1)x(2*r+1)
        int r = option.Despeckle;
        int a2 = (2 * r + 1) * (2 * r + 1) / 2 + 1;
        size_t k = 0;
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
                            size_t kf = width * yf + xf;
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

static bool NSHPaletteGenerate(NSHRgb *samples, size_t samplesSize, NSHRgb bgColor, NSHOption option, NSHRgb *palette, size_t paletteSize)
{
    bool* fgMask = NULL;
    if (!(fgMask = (bool*)malloc(samplesSize * sizeof(bool))))
    {
        return false;
    }
    ForegroundMaskCreate(bgColor, samples, samplesSize, option, fgMask);
    size_t dataSize = 0;
    for (size_t i = 0; i < samplesSize; i++)
    {
        if (fgMask[i])
        {
            dataSize++;
        }
    }
    NSHRgb* data = NULL;
    if (!(data = (NSHRgb*)malloc(dataSize * sizeof(NSHRgb))))
    {
        return false;
    }
    size_t k = 0;
    for (size_t i = 0; i < samplesSize; i++)
    {
        if (fgMask[i])
        {
            data[k] = samples[i];
            k++;
        }
    }

    NSHRgb* means = NULL;
    if (!(means = (NSHRgb*)malloc((paletteSize - 1) * sizeof(NSHRgb))))
    {
        return false;
    }
    ImageKMeans(data, dataSize, means, paletteSize - 1, option.KmeansMaxIter);

    size_t idx = 0;
    palette[0] = bgColor;
    for (size_t i = 1; i < paletteSize; i++)
    {
        palette[i] = means[i - 1];
    }
    free(fgMask);
    free(data);
    free(means);

    return true;
}

NOTESHRINKAPI NSHOption NSHMakeDefaultOption()
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

NOTESHRINKAPI bool NSHPaletteCreate(NSHRgb* input, size_t inputSize, NSHOption option, NSHRgb* palette, size_t paletteSize)
{
    if (!input || !palette)
    {
        return false;
    }
    if (option.NumColors < 2)
    {
        return false;
    }
    size_t samplesSize = (size_t)(option.SampleFraction * inputSize);
    if ((samplesSize < 0) || (samplesSize > inputSize))
    {
        return false;
    }

    NSHRgb* samples = NULL;
    if (!(samples = (NSHRgb*)malloc(samplesSize * sizeof(NSHRgb))))
    {
        return false;
    }
    samplesSize = ImageSamplePixels(input, inputSize, samples, samplesSize, option);
    NSHRgb bgColor = BackgroundColorFind(samples, samplesSize, bitsPerSample);
    NSHPaletteGenerate(samples, samplesSize, bgColor, option, palette, paletteSize);
    free(samples);

    return true;
}

NOTESHRINKAPI bool NSHPaletteApply(NSHRgb* img, size_t imgSize, NSHRgb* palette, size_t paletteSize, int width, int height, NSHOption option, uint8_t *result)
{
    bool* fgMask = NULL;
    if (!(fgMask = (bool*)malloc(imgSize * sizeof(bool))))
    {
        return false;
    }
    NSHRgb bgColor = palette[0];
    ForegroundMaskCreate(bgColor, img, imgSize, option, fgMask);
    ForegroundMaskDespeckle(fgMask, width, height, option);
    for (size_t i = 0; i <  imgSize; i++)
    {
        if (!fgMask[i])
        {
            result[i] = 0;
        }
        else
        {
            NSHRgb p = img[i];
            int minIdx = NSHRgbClosest(p, palette, paletteSize);
            result[i] = minIdx;
        }
    }
    free(fgMask);

    return true;
}

NOTESHRINKAPI bool NSHPaletteSaturate(NSHRgb *palette, size_t paletteSize)
{
    float maxSat = 0.0f, minSat = 1.0f, ks = 1.0f;

    for (size_t i = 0; i < paletteSize; i++)
    {
        NSHRgb hsv = ColorRgbToHsv(palette[i]);
        float s = hsv.G;
        if (maxSat < s) maxSat = s;
        if (minSat > s) minSat = s;
    }
    if (maxSat > minSat)
    {
        ks = 1.0f / (maxSat - minSat);
        for (size_t i = 0; i < paletteSize; i++)
        {
            NSHRgb hsv = ColorRgbToHsv(palette[i]);
            float s = hsv.G;
            float newSat = (s - minSat) * ks;
            hsv.G = newSat;
            palette[i] = ColorHsvToRgb(hsv);
        }
    }

    return true;
}

NOTESHRINKAPI bool NSHPaletteNorm(NSHRgb *palette, size_t paletteSize)
{
    float maxVal = 0.0f, minVal = 1.0f, kv = 1.0f;

    for (size_t i = 0; i < paletteSize; i++)
    {
        NSHRgb hsv = ColorRgbToHsv(palette[i]);
        float v = hsv.B;
        if (maxVal < v) maxVal = v;
        if (minVal > v) minVal = v;
    }
    if (maxVal > minVal)
    {
        kv = 1.0f / (maxVal - minVal);
        for (size_t i = 0; i < paletteSize; i++)
        {
            NSHRgb hsv = ColorRgbToHsv(palette[i]);
            float v = hsv.B;
            float newVal = (v - minVal) * kv;
            hsv.B = newVal;
            palette[i] = ColorHsvToRgb(hsv);
        }
    }

    return true;
}
