#include <string>
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
    int MinInt = 2;
    float MinFloat = 0.001;
    NSHOption o = NSHMakeDefaultOption();
    int fhelp = 0;
    if (argc < 3)
    {
        NoteshrinkUsage(argv[0], o);
        return 0;
    }
    else
    {
        int fquiet;
        int opt;
        while ((opt = getopt(argc, argv, ":k:n:p:qr:s:v:wNSh")) != -1)
        {
            switch(opt)
            {
            case 'k':
                o.KmeansMaxIter = atoi(optarg);
                if (o.KmeansMaxIter < MinInt)
                {
                    printf("ERROR: NUM_ITERS = %d < %d", o.KmeansMaxIter, MinInt);
                    return 1;
                }
                break;
            case 'n':
                o.NumColors = atoi(optarg);
                if (o.NumColors < MinInt)
                {
                    printf("ERROR: NUM_COLORS = %d < %d", o.NumColors, MinInt);
                    return 1;
                }
                break;
            case 'p':
                o.SampleFraction = atof(optarg);
                if (o.SampleFraction < MinFloat)
                {
                    printf("ERROR: p NUM = %f < %f", o.SampleFraction, MinFloat);
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
                    printf("ERROR: NUM_COLORS = %d < %d", o.Despeckle, 0);
                    return 1;
                }
                break;
            case 's':
                o.SaturationThreshold = atof(optarg);
                if (o.SaturationThreshold < MinFloat)
                {
                    printf("ERROR: s NUM = %f < %f", o.SaturationThreshold, MinFloat);
                    return 1;
                }
                break;
            case 'v':
                o.BrightnessThreshold = atof(optarg);
                if (o.BrightnessThreshold < MinFloat)
                {
                    printf("ERROR: v NUM = %f < %f", o.BrightnessThreshold, MinFloat);
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
                printf("option needs a value\n");
                return 2;
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                return 3;
                break;
            }
        }
        if(optind + 2 > argc || fhelp)
        {
            NoteshrinkUsage(argv[0], o);
            return 0;
        }
        std::string filein(argv[optind]);
        std::string fileout(argv[optind + 1]);

        int width, height, bpp;
        stbi_uc* pixels = stbi_load(filein.c_str(), &width, &height, &bpp, STBI_rgb_alpha);
        size_t pixs = width * height;
        NSHRgb* img = (NSHRgb*)malloc(pixs * sizeof(NSHRgb));

        size_t i = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                size_t idx = (height - y - 1) * width * 4 + x * 4;
                NSHRgb p;
                p.R = pixels[idx];
                p.G = pixels[idx + 1];
                p.B = pixels[idx + 2];
                img[i++] = p;
            }
        }
        free(pixels);

        NSHRgb* palette = (NSHRgb*)malloc(o.NumColors * sizeof(NSHRgb));
        uint8_t* result = (uint8_t*)malloc(pixs * sizeof(uint8_t));
        NSHPaletteCreate(img, pixs, o, palette, o.NumColors);
        NSHPaletteApply(img, pixs, palette, o.NumColors, width, height, o, result);
        if (o.Saturate)
        {
            NSHPaletteSaturate(palette,o.NumColors);
        }
        if (o.Norm)
        {
            NSHPaletteNorm(palette, o.NumColors);
        }
        if (o.WhiteBackground)
        {
            palette[0] = NSHRgb{ 255, 255, 255 };
        }
        if (!fquiet)
        {
            printf("Palette:\n");
            for (i = 0; i < o.NumColors; i++)
            {
                uint8_t r = (uint8_t)(palette[i].R + 0.5f);
                uint8_t g = (uint8_t)(palette[i].G + 0.5f);
                uint8_t b = (uint8_t)(palette[i].B + 0.5f);
                printf("%d: #%02x%02x%02x\n", i, r, g, b);
            }
        }

        int numberOfChannels = 3;
        uint8_t* data = (uint8_t*)malloc(pixs * numberOfChannels * sizeof(uint8_t));

        i = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                size_t idx = (height - y - 1) * width * numberOfChannels + x * numberOfChannels;
                NSHRgb p = palette[result[i++]];
                data[idx] = (uint8_t)(p.R + 0.5f);
                data[idx + 1] = (uint8_t)(p.G + 0.5f);
                data[idx + 2] = (uint8_t)(p.B + 0.5f);
            }
        }
        stbi_write_png(fileout.c_str(), width, height, numberOfChannels, data, width * numberOfChannels);
        if (!fquiet)
        {
            printf("done\n");
        }
        free(data);
        free(result);
        free(palette);
        free(img);
    }
    return 0;
}
