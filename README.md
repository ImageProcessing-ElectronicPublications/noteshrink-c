# noteshrink

Convert scans of handwritten notes to beautiful.

## Requirements

 - [stb](https://github.com/nothings/stb). Copy to `src/ext`.

## Usage

```shell
./noteshrink IMAGE_IN IMAGE_OUT.png
```

## Build

```shell
mkdir build
cd build
cmake ..
make
```

## Demo

```shell
./noteshrink -n 8 ../images/test.jpg ../images/testout.png
Palette:
0: #e6e6b6
1: #d44e4f
2: #90907f
3: #42423f
4: #619c83
5: #438a73
6: #636458
7: #e78178
Save png: ../images/testout.png
done
```
```shell
optipng -q ../images/testout.png
```

`images/test.jpg`:  
![test](images/test.jpg)
**35113** bytes.

`images/testout.png`:  
![testout](images/testout.png)
**7281** bytes.

See all imageset: [noteshrink-demo](https://github.com/ImageProcessing-ElectronicPublications/noteshrink-demo).
