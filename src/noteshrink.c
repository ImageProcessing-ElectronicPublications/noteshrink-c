#include "noteshrink.h"

#define bitsPerSample 6

static float NSHSquareDistance(float *a, float *b, int channels)
{
    int d, dChannels;
    float squareDistance, delta;

    dChannels = (channels < 3) ? channels : 3;
    squareDistance = 0.0f;
    for (d = 0; d < dChannels; d++)
    {
        delta = a[d] - b[d];
        squareDistance += (delta * delta);
    }

    return squareDistance;
}

static size_t NSHClosest(float *p, float *means, size_t meansSize, int channels)
{
    int d, dChannels;
    float minimum, squaredDistance, pm[3];
    size_t idx, i, k;

    dChannels = (channels < 3) ? channels : 3;
    minimum = 255.0f * 255.0f * channels;
    idx = 0;
    k = 0;
    for (i = 0; i < meansSize; i++)
    {
        for (d = 0; d < dChannels; d++)
        {
            pm[d] = means[k + d];
        }
        k += channels;
        squaredDistance = NSHSquareDistance(p, pm, channels);
        if (squaredDistance < minimum)
        {
            minimum = squaredDistance;
            idx = i;
        }
    }

    return idx;
}

static size_t NSHClosestD(float *p, unsigned char *data, size_t dataSize, int channels)
{
    int d, dChannels;
    float minimum, squaredDistance, pm[3];
    size_t idx, i, k;

    dChannels = (channels < 3) ? channels : 3;
    minimum = 255.0f * 255.0f * channels;
    idx = 0;
    k = 0;
    for (i = 0; i < dataSize; i++)
    {
        for (d = 0; d < dChannels; d++)
        {
            pm[d] = (float)data[k + d];
        }
        k += channels;
        squaredDistance = NSHSquareDistance(p, pm, channels);
        if (squaredDistance < minimum)
        {
            minimum = squaredDistance;
            idx = i;
        }
    }

    return idx;
}

static void ColorRgbToHsv(float *rgb)
{
    int d, imi, ima;
    float hsv[3], max, min;

    max = -1.0f;
    min = 2.0f;
    imi = 0;
    ima = 0;
    for (d = 0; d < 3; d++)
    {
        rgb[d] /= 255.0f;
        if (max < rgb[d])
        {
            ima = d;
            max = rgb[ima];
        }
        if (min > rgb[d])
        {
            imi = d;
            min = rgb[imi];
        }
    }
    hsv[0] = max - min;
    if (hsv[0] > 0.0f)
    {
        if (ima == 0)
        {
            hsv[0] = (rgb[1] - rgb[2]) / hsv[0];
            if (hsv[0] < 0.0f)
            {
                hsv[0] += 6.0f;
            }
        }
        else if (ima == 1)
        {
            hsv[0] = 2.0f + (rgb[2] - rgb[0]) / hsv[0];
        }
        else
        {
            hsv[0] = 4.0f + (rgb[0] - rgb[1]) / hsv[0];
        }
    }
    hsv[0] /= 6.0f;
    hsv[1] = max - min;
    if (max > 0.0f)
    {
        hsv[1] /= max;
    }
    hsv[2] = max;
    for (d = 0; d < 3; d++)
    {
        rgb[d] = hsv[d];
    }
}

static void ColorHsvToRgb(float *hsv)
{
    int i, d;
    float rgb[3], f;

    for (d = 0; d < 3; d++)
    {
        rgb[d] = hsv[2];
    }
    if (hsv[1] > 0.0f)
    {
        hsv[0] *= 6.0f;
        i = (int)hsv[0];
        f = hsv[0] - (float)i;
        switch (i)
        {
            default:
            case 0:
                rgb[1] *= (1.0f - hsv[1] * (1.0f - f));
                rgb[2] *= (1.0f - hsv[1]);
                break;
            case 1:
                rgb[0] *= (1.0f - hsv[1] * f);
                rgb[2] *= (1.0f - hsv[1]);
                break;
            case 2:
                rgb[0] *= (1.0f - hsv[1]);
                rgb[2] *= (1.0f - hsv[1] * (1.0f - f));
                break;
            case 3:
                rgb[0] *= (1.0f - hsv[1]);
                rgb[1] *= (1.0f - hsv[1] * f);
                break;
            case 4:
                rgb[0] *= (1.0f - hsv[1] * (1.0f - f));
                rgb[1] *= (1.0f - hsv[1]);
                break;
            case 5:
                rgb[1] *= (1.0f - hsv[1]);
                rgb[2] *= (1.0f - hsv[1] * f);
                break;
        }
    }
    for (d = 0; d < 3; d++)
    {
        hsv[d] = rgb[d] * 255.0f;
    }
}

static size_t ImageSamplePixels(unsigned char *img, size_t imgSize, int channels, unsigned char *samples, size_t samplesSize, NSHOption o)
{
    int d;
    size_t k, interval, i;

    k = 0;
    if ((samplesSize > 0) && (samplesSize < imgSize))
    {
        interval = (size_t)imgSize / samplesSize;
        interval = (interval > 0) ? interval : 1;
        for (i = 0; i < imgSize; i += interval)
        {
            if (k < samplesSize)
            {
                for (d = 0; d < channels; d++)
                {
                    samples[k * channels + d] = img[i * channels + d];
                }
                k++;
            }
        }
    }
    return k;
}

static void ImageQuantize(unsigned char *img, size_t imageSize, int channels, int bitsPerChannel, unsigned int *quantized)
{
    unsigned char shift, halfbin, d;
    unsigned int c, p, qChannels;
    size_t i, k;

    shift = 8 - bitsPerChannel;
    halfbin = (((unsigned char)1 << shift) >> 1);
    qChannels = (channels < 3) ? channels : 3;

    k = 0;
    for (i = 0; i < imageSize; i++)
    {
        p = 0;
        for (d = 0; d < qChannels; d++)
        {
            p <<= 8;
            c = ((img[k + d] >> shift) << shift) + halfbin;
            p = p | c;
        }
        quantized[i] = p;
        k += channels;
    }
}

static bool ImageKMeans(unsigned char *data, size_t dataSize, int channels, float* means, int k, int maxItr)
{
    int pChannels, mChannels, d, itr, cluster, changes;
    float h, p[3], fk, fksq;
    size_t i, l, n;
    int *clusters = NULL;
    int *mLen = NULL;

    fk = (k > 0) ? (1.0f / (float)k) : 0.0f;
    fksq = sqrt(fk);
    pChannels = 3;
    mChannels = (channels < pChannels) ? channels : pChannels;
    for (i = 0; i < k; i++)
    {
        h = ((float)i + 0.5f) * fk;
        p[0] = h;
        p[1] = 1.0f;
        p[2] = 1.0f;
        ColorHsvToRgb(p);
        l = NSHClosestD(p, data, dataSize, channels);
        for (d = 0; d < mChannels; d++)
        {
            means[i * channels + d] = (float)data[l * channels + d];
        }
    }

    if (!(clusters = (int*)malloc(dataSize * sizeof(int))))
    {
        return false;
    }
    n = 0;
    for (i = 0; i < dataSize; i++)
    {
        for (d = 0; d < mChannels; d++)
        {
            p[d] = data[n + d];
        }
        clusters[i] = NSHClosest(p, means, k, channels);
        n += channels;
    }

    if (!(mLen = (int*)malloc(k * sizeof(int))))
    {
        return false;
    }
    for (itr = 0; itr < maxItr; itr++)
    {
        n = 0;
        for (i = 0; i < k; i++)
        {
            for (d = 0; d < mChannels; d++)
            {
                means[n + d] = 0.0f;
            }
            n += channels;
            mLen[i] = 0;
        }
        n = 0;
        for (i = 0; i < dataSize; i++)
        {
            cluster = clusters[i];
            for (d = 0; d < mChannels; d++)
            {
                means[cluster * channels + d] += (float)data[n + d];
            }
            mLen[cluster]++;
            n += channels;
        }
        changes = 0;
        n = 0;
        for (i = 0; i < k; i++)
        {
            if (mLen[i] > 0)
            {
                for (d = 0; d < mChannels; d++)
                {
                    means[n + d] /= (float)mLen[i];
                }
            }
            else
            {
                h = ((float)i + 0.5f) * fk;
                p[0] = h;
                p[1] = 1.0f;
                p[2] = 1.0f;
                ColorHsvToRgb(p);
                l = NSHClosest(p, means, k, channels);
                for (d = 0; d < mChannels; d++)
                {
                    p[d] *= fksq;
                    p[d] += (means[l * channels + d] * (1.0f - fksq));
                }
                l = NSHClosestD(p, data, dataSize, channels);
                for (d = 0; d < mChannels; d++)
                {
                    means[n + d] += (float)data[l * channels + d];
                }
                mLen[i] = 1;
                changes++;
            }
            n += channels;
        }
        n = 0;
        for (i = 0; i < dataSize; i++)
        {
            for (d = 0; d < mChannels; d++)
            {
                p[d] = (float)data[n + d];
            }
            n += channels;
            cluster = NSHClosest(p, means, k, channels);
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

static bool BGColorFind(unsigned char *image, size_t imageSize, int channels, float *palette, int paletteSize, int bitsPerChannel)
{
    unsigned char shift, r, g, b;
    unsigned int maxcount, maxvalue, palChannels, d;
    size_t i, j;
    unsigned int *quantized = NULL;
    int *count = NULL;

    if (!(quantized = (unsigned int*)malloc(imageSize * sizeof(unsigned int))))
    {
        return false;
    }
    ImageQuantize(image, imageSize, channels, bitsPerChannel, quantized);
    if (!(count = (int*)malloc(imageSize * sizeof(int))))
    {
        return false;
    }
    for (i = 0; i < imageSize; i++)
    {
        count[i] = 1;
    }
    for (i = 0; i < imageSize - 1; i++)
    {
        if (count[i] > 0)
        {
            for (j = i + 1; j < imageSize; j++)
            {
                if (quantized[i] == quantized[j])
                {
                    count[i] += count[j];
                    count[j] = 0;
                }
            }
        }
    }
    maxcount = count[0];
    maxvalue = quantized[0];
    for (i = 1; i < imageSize; i++)
    {
        if (maxcount < count[i])
        {
            maxcount = count[i];
            maxvalue = quantized[i];
        }
    }

    shift = 8 - bitsPerChannel;
    for (d = 0; d < channels; d++)
    {
        palette[channels - 1 - d] = maxvalue & 0xff;
        maxvalue >>= 8;
    }
    free(quantized);
    free(count);

    return true;
}

static void FGMaskCreate(unsigned char *samples, size_t samplesSize, int channels, float *palette, int paletteSize, NSHOption option, bool *mask)
{
    int d, mChannels;
    float bghsv[3], hsv[3], sd, vd;
    size_t i, k;

    mChannels = (channels < 3) ? channels : 3;
    if (mChannels < 3)
    {
        for (d = 0; d < 3; d++)
        {
            bghsv[d] = palette[0];
        }
    }
    else
    {
        for (d = 0; d < 3; d++)
        {
            bghsv[d] = palette[d];
        }
    }
    ColorRgbToHsv(bghsv);
    k = 0;
    for (i = 0; i < samplesSize; i++)
    {
        if (mChannels < 3)
        {
            for (d = 0; d < 3; d++)
            {
                hsv[d] = (float)samples[k];
            }
        }
        else
        {
            for (d = 0; d < 3; d++)
            {
                hsv[d] = (float)samples[k + d];
            }
        }
        k += channels;
        ColorRgbToHsv(hsv);
        sd = (bghsv[1] > hsv[1]) ? (bghsv[1] - hsv[1]) : (hsv[1] - bghsv[1]);
        vd = (bghsv[2] > hsv[2]) ? (bghsv[2] - hsv[2]) : (hsv[2] - bghsv[2]);
        mask[i] = ((vd >= option.BrightnessThreshold) || (sd >= option.SaturationThreshold));
    }
}

static void FGMaskDespeckle(bool* fgMask, int width, int height, NSHOption option)
{
    int r, a2, y, x, l, i, j, yf, xf;
    size_t k, kf;

    if (option.Despeckle > 0)
    {
        // Despeckle (2*r+1)x(2*r+1)
        r = option.Despeckle;
        a2 = (2 * r + 1) * (2 * r + 1) / 2 + 1;
        k = 0;
        for (y = 0; y < height; y++)
        {
            for (x = 0; x < width; x++)
            {
                if (!fgMask[k])
                {
                    l = 0;
                    for (i = -r; i < (r + 1); i++)
                    {
                        yf = y + i;
                        yf = (yf < 0) ? 0 : ((yf < height) ? yf : (height - 1));
                        for (j = -r; j < (r + 1); j++)
                        {
                            xf = x + j;
                            xf = (xf < 0) ? 0 : ((xf < width) ? xf : (width - 1));
                            kf = width * yf + xf;
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

static bool NSHPaletteGenerate(unsigned char *samples, size_t samplesSize, int channels, NSHOption option, float *palette, size_t paletteSize)
{
    int d;
    size_t dataSize, i, k, ks;
    bool *fgMask = NULL;
    unsigned char *data = NULL;
    float *means = NULL;

    if (!(fgMask = (bool*)malloc(samplesSize * sizeof(bool))))
    {
        return false;
    }
    FGMaskCreate(samples, samplesSize, channels, palette, paletteSize, option, fgMask);
    dataSize = 0;
    for (i = 0; i < samplesSize; i++)
    {
        if (fgMask[i])
        {
            dataSize++;
        }
    }
    if (!(data = (unsigned char*)malloc(dataSize * channels * sizeof(unsigned char))))
    {
        return false;
    }
    if (!(means = (float*)malloc((paletteSize - 1) * channels * sizeof(float))))
    {
        return false;
    }

    k = 0;
    ks = 0;
    for (i = 0; i < samplesSize; i++)
    {
        if (fgMask[i])
        {
            for (d = 0; d < channels; d++)
            {
                data[k] = samples[ks + d];
                k++;
            }
        }
        ks += channels;
    }

    ImageKMeans(data, dataSize, channels, means, paletteSize - 1, option.KmeansMaxIter);

    k = 0;
    for (i = 0; i < (paletteSize - 1); i++)
    {
        for (d = 0; d < channels; d++)
        {
            palette[k + channels] = means[k];
            k++;
        }
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

NOTESHRINKAPI bool NSHPaletteCreate(unsigned char *img, int height, int width, int channels, NSHOption option, float *palette, int paletteSize)
{
    unsigned char *samples = NULL;
    size_t imgSize, samplesSize;

    if (!img || !palette)
    {
        return false;
    }
    if (option.NumColors < 2)
    {
        return false;
    }
    imgSize = height * width;
    samplesSize = (size_t)(option.SampleFraction * imgSize);
    if ((samplesSize == 0) || (samplesSize > imgSize))
    {
        return false;
    }

    if (!(samples = (unsigned char*)malloc(samplesSize * channels * sizeof(unsigned char))))
    {
        return false;
    }
    samplesSize = ImageSamplePixels(img, imgSize, channels, samples, samplesSize, option);
    BGColorFind(samples, samplesSize, channels, palette, paletteSize, bitsPerSample);
    NSHPaletteGenerate(samples, samplesSize, channels, option, palette, paletteSize);
    free(samples);

    return true;
}

NOTESHRINKAPI bool NSHPaletteApply(unsigned char *img, int height, int width, int channels, float *palette, int paletteSize, NSHOption option, unsigned char *result)
{
    int d,  mChannels, minIdx;
    float p[3];
    size_t i, k, imgSize;
    bool* fgMask = NULL;

    imgSize = height * width;
    if (!(fgMask = (bool*)malloc(imgSize * sizeof(bool))))
    {
        return false;
    }
    FGMaskCreate(img, imgSize, channels, palette, paletteSize, option, fgMask);
    FGMaskDespeckle(fgMask, width, height, option);
    mChannels = (channels < 3) ? channels : 3;
    k = 0;
    for (i = 0; i <  imgSize; i++)
    {
        if (!fgMask[i])
        {
            result[i] = 0;
        }
        else
        {
            for (d = 0; d < mChannels; d++)
            {
                p[d] = (float)img[k + d];
            }
            minIdx = NSHClosest(p, palette, paletteSize, channels);
            result[i] = (unsigned char)minIdx;
        }
        k += channels;
    }
    free(fgMask);

    return true;
}

NOTESHRINKAPI bool NSHPaletteSaturate(float *palette, int paletteSize, int channels)
{
    int d, i, k, mChannels;
    float p[3], maxSat, minSat, ks;

    maxSat = 0.0f;
    minSat = 1.0f;
    ks = 1.0f;
    mChannels = (channels < 3) ? channels : 3;
    k = 0;
    for (i = 0; i < paletteSize; i++)
    {
        if (mChannels < 3)
        {
            for (d = 0; d < 3; d++)
            {
                p[d] = palette[k];
            }
        }
        else
        {
            for (d = 0; d < 3; d++)
            {
                p[d] = palette[k + d];
            }
        }
        ColorRgbToHsv(p);
        if (maxSat < p[1]) maxSat = p[1];
        if (minSat > p[1]) minSat = p[1];
        k += channels;
    }
    if (maxSat > minSat)
    {
        ks = 1.0f / (maxSat - minSat);
        k = 0;
        for (i = 0; i < paletteSize; i++)
        {
            if (mChannels < 3)
            {
                for (d = 0; d < 3; d++)
                {
                    p[d] = palette[k];
                }
            }
            else
            {
                for (d = 0; d < 3; d++)
                {
                    p[d] = palette[k + d];
                }
            }
            ColorRgbToHsv(p);
            p[1] = (p[1] - minSat) * ks;
            ColorHsvToRgb(p);
            for (d = 0; d < mChannels; d++)
            {
                palette[k + d] = p[d];
            }
            k += channels;
        }
    }

    return true;
}

NOTESHRINKAPI bool NSHPaletteNorm(float *palette, int paletteSize, int channels)
{
    int d, i, k, mChannels;
    float p[3], maxVal, minVal, kv;

    maxVal = 0.0f;
    minVal = 1.0f;
    kv = 1.0f;
     mChannels = (channels < 3) ? channels : 3;
    k = 0;
    for (i = 0; i < paletteSize; i++)
    {
        if ( mChannels < 3)
        {
            for (d = 0; d < 3; d++)
            {
                p[d] = palette[k];
            }
        }
        else
        {
            for (d = 0; d < 3; d++)
            {
                p[d] = palette[k + d];
            }
        }
        ColorRgbToHsv(p);
        if (maxVal < p[2]) maxVal = p[2];
        if (minVal > p[2]) minVal = p[2];
        k += channels;
    }
    if (maxVal > minVal)
    {
        kv = 1.0f / (maxVal - minVal);
        k = 0;
        for (i = 0; i < paletteSize; i++)
        {
            if ( mChannels < 3)
            {
                for (d = 0; d < 3; d++)
                {
                    p[d] = palette[k];
                }
            }
            else
            {
                for (d = 0; d < 3; d++)
                {
                    p[d] = palette[k + d];
                }
            }
            ColorRgbToHsv(p);
            p[2] = (p[2] - minVal) * kv;
            ColorHsvToRgb(p);
            for (d = 0; d < mChannels; d++)
            {
                palette[k + d] = p[d];
            }
            k += channels;
        }
    }

    return true;
}
