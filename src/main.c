#include <unistd.h>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include "noteshrink.h"

void NoteshrinkUsage(char* prog, NSHOption o)
{
    printf("NoteShrink version %s.\n", NOTESHRINK_VERSION);
    printf("usage: %s [options] image_in image_out.png\n", prog);
    printf("options:\n");
    printf("  -k NUM_ITERS      number of iterations KMeans (default %d)\n", o.KmeansMaxIter);
    printf("  -n NUM_COLORS     number of output colors (default %d)\n", o.NumColors);
    printf("  -p NUM            part of pixels to sample (default %f)\n", o.SampleFraction);
    printf("  -q                quiet mode\n");
    printf("  -r RADIUS         radius despeckle FG mask (default %d)\n", o.Despeckle);
    printf("  -s NUM            background saturation threshold (default %f)\n", o.SaturationThreshold);
    printf("  -v NUM            background value threshold (default %f)\n", o.BrightnessThreshold);
    printf("  -w                make background white (default %d)\n", o.WhiteBackground);
    printf("  -N                on/off normalize colors (default %d)\n", o.Norm);
    printf("  -S                on/off saturate colors (default %d)\n", o.Saturate);
    printf("  -h                show this help message and exit\n");
}

int main(int argc, char **argv)
{
    int height, width, channels, mChannels, y, x, d;
    size_t ki, kd;
    unsigned char *data = NULL, *result = NULL;
    float *palette = NULL;
    stbi_uc *img = NULL;

    int MinInt = 2;
    float MinFloat = 0.001;
    NSHOption o = NSHMakeDefaultOption();
    int fquiet = 0;
    int fhelp = 0;

    fhelp = (argc < 3) ? 1 : fhelp;
    int opt;
    while ((opt = getopt(argc, argv, ":k:n:p:qr:s:v:wNSh")) != -1)
    {
        switch(opt)
        {
        case 'k':
            o.KmeansMaxIter = atoi(optarg);
            if (o.KmeansMaxIter < MinInt)
            {
                fprintf(stderr, "ERROR: NUM_ITERS = %d < %d", o.KmeansMaxIter, MinInt);
                return 1;
            }
            break;
        case 'n':
            o.NumColors = atoi(optarg);
            if (o.NumColors < MinInt)
            {
                fprintf(stderr, "ERROR: NUM_COLORS = %d < %d", o.NumColors, MinInt);
                return 1;
            }
            break;
        case 'p':
            o.SampleFraction = atof(optarg);
            if (o.SampleFraction < MinFloat)
            {
                fprintf(stderr, "ERROR: p NUM = %f < %f", o.SampleFraction, MinFloat);
                return 1;
            }
            break;
        case 'q':
            fquiet = 1;
            break;
        case 'r':
            o.Despeckle = atoi(optarg);
            if (o.Despeckle < 0)
            {
                fprintf(stderr, "ERROR: NUM_COLORS = %d < %d", o.Despeckle, 0);
                return 1;
            }
            break;
        case 's':
            o.SaturationThreshold = atof(optarg);
            if (o.SaturationThreshold < MinFloat)
            {
                fprintf(stderr, "ERROR: s NUM = %f < %f", o.SaturationThreshold, MinFloat);
                return 1;
            }
            break;
        case 'v':
            o.BrightnessThreshold = atof(optarg);
            if (o.BrightnessThreshold < MinFloat)
            {
                fprintf(stderr, "ERROR: v NUM = %f < %f", o.BrightnessThreshold, MinFloat);
                return 1;
            }
            break;
        case 'w':
            o.WhiteBackground = !o.WhiteBackground;
            break;
        case 'S':
            o.Saturate = !o.Saturate;
            break;
        case 'N':
            o.Norm = !o.Norm;
            break;
        case 'h':
            fhelp = 1;
            break;
        case ':':
            fprintf(stderr, "ERROR: option needs a value\n");
            return 2;
            break;
        case '?':
            fprintf(stderr, "ERROR: unknown option: %c\n", optopt);
            return 3;
            break;
        }
    }
    if(optind + 2 > argc || fhelp)
    {
        NoteshrinkUsage(argv[0], o);
        return 0;
    }
    char *filein = argv[optind];
    char *fileout = argv[optind + 1];

    if (!(img = stbi_load(filein, &width, &height, &channels, STBI_rgb_alpha)))
    {
        fprintf(stderr, "ERROR: not read image: %s\n", filein);
        return 5;
    }
    printf("image: %dx%d:%d\n", width, height, channels);
    if (!(data = (unsigned char*)malloc(height * width * channels * sizeof(unsigned char))))
    {
        fprintf(stderr, "ERROR: not use memmory\n");
        return 1;
    }
    ki = 0;
    kd = 0;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            for (d = 0; d < channels; d++)
            {
                data[kd + d] = (unsigned char)img[ki + d];
            }
            ki += STBI_rgb_alpha;
            kd += channels;
        }
    }
    stbi_image_free(img);


    if (!(palette = (float*)malloc(o.NumColors * channels * sizeof(float))))
    {
        fprintf(stderr, "ERROR: not use memmory\n");
        return 4;
    }
    if (!(result = (unsigned char*)malloc(height * width * sizeof(uint8_t))))
    {
        fprintf(stderr, "ERROR: not use memmory\n");
        return 4;
    }

    NSHPaletteCreate(data, height, width, channels, o, palette, o.NumColors);
    NSHPaletteApply(data, height, width, channels, palette, o.NumColors, o, result);
    if (o.Saturate)
    {
        NSHPaletteSaturate(palette, o.NumColors, channels);
    }
    if (o.Norm)
    {
        NSHPaletteNorm(palette, o.NumColors, channels);
    }
    if (o.WhiteBackground)
    {
        for (d = 0; d < channels; d++)
        {
            palette[d] = 255.0f;
        }
    }
    kd = 0;
    for (ki = 0; ki < o.NumColors; ki++)
    {
        for (d = 0; d < channels; d++)
        {
            palette[kd] += 0.5f;
            palette[kd] = (palette[kd] < 0) ? 0.0f : (palette[kd] < 255.0f) ? palette[kd] : 255.0f;
            kd++;
        }
    }
    if (!fquiet)
    {
        printf("Palette:\n");
        kd = 0;
        for (ki = 0; ki < o.NumColors; ki++)
        {
            printf("%d: #", ki);
            for (d = 0; d < channels; d++)
            {
                printf("%02x", (unsigned char)palette[kd]);
                kd++;
            }
            printf("\n");
        }
    }

    ki = 0;
    kd = 0;
    mChannels = (channels < 3) ? channels : 3;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            for (d = 0; d < mChannels; d++)
            {
                data[kd + d] = (unsigned char)palette[(int)result[ki] * channels + d];
            }
            kd += channels;
            ki++;
        }
    }
    if (!fquiet) printf("Save png: %s\n", fileout);
    if (!(stbi_write_png(fileout, width, height, channels, data, width * channels)))
    {
        fprintf(stderr, "ERROR: not write image: %s\n", fileout);
        return 5;
    }
    if (!fquiet) printf("done\n");
    free(data);
    free(result);
    free(palette);

    return 0;
}
