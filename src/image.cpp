/*
 Copyright © 2024 Insoft. All rights reserved.
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#include "image.hpp"

#include <fstream>

/* Windows 3.x bitmap file header */
typedef struct __attribute__((__packed__)) {
    char      bfType[2];   /* magic - always 'B' 'M' */
    uint32_t  bfSize;
    uint16_t  bfReserved1;
    uint16_t  bfReserved2;
    uint32_t  bfOffBits;    /* offset in bytes to actual bitmap data */
} BMPHeader;

/* Windows 3.x bitmap full header, including file header */

typedef struct __attribute__((__packed__)) {
    BMPHeader fileHeader;
    uint32_t  biSize;
    int32_t   biWidth;
    int32_t   biHeight;
    int16_t   biPlanes;           // Number of colour planes, set to 1
    int16_t   biBitCount;         // Colour bits per pixel. 1 4 8 24 or 32
    uint32_t  biCompression;      // *Code for the compression scheme
    uint32_t  biSizeImage;        // *Size of the bitmap bits in bytes
    int32_t   biXPelsPerMeter;    // *Horizontal resolution in pixels per meter
    int32_t   biYPelsPerMeter;    // *Vertical resolution in pixels per meter
    uint32_t  biClrUsed;          // *Number of colours defined in the palette
    uint32_t  biClImportant;      // *Number of important colours in the image
} BIPHeader;

template <typename T>
static T swap_endian(T u)
{
    static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

    union
    {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

static void flipImageVertically(const TImage image)
{
    if (!image.data) return;
    
    uint8_t *byte = (uint8_t *)image.data;
    
    int w = (int)image.width;
    int h = (int)image.height;
    
    w = w / (8 / static_cast<int>(image.bitWidth));
    
    for (int row = 0; row < h / 2; ++row)
        for (int col = 0; col < w; ++col)
            std::swap(byte[col + row * w], byte[col + (h - 1 - row) * w]);
    
}

static void unpack4BitValues(uint8_t *dst, const uint8_t *src, size_t length)
{
    union {
        struct {
            uint8_t l: 4;
            uint8_t h: 4;
        };
        uint8_t v;
    } byte;
    
    int i, j;
    for (i = (int)length - 1, j = (int)length * 2 - 1; i >= 0; --i, j-=2) {
        byte.v = src[i];
        dst[j] = byte.l;
        dst[j-1] = byte.h;
    }
}

static bool isBMP(std::string &filename)
{
    std::ifstream infile;
    
    infile.open(filename, std::ios::in | std::ios::binary);
    if (!infile.is_open())
        return false;
    
    
    BIPHeader bip_header;
    infile.read((char *)&bip_header, sizeof(BIPHeader));
    infile.close();
    
    if (strncmp(bip_header.fileHeader.bfType, "BM", 2) != 0)
        return false;

    return true;
}

static bool isPBM(std::string &filename)
{
    std::ifstream infile;
    
    infile.open(filename, std::ios::in | std::ios::binary);
    if (!infile.is_open())
        return false;
    
    std::string s;
    getline(infile, s);
    infile.close();
    
    if (s != "P4")
        return false;
    
    return true;
}

/**
 @brief    Loads a file in the Bitmap (BMP) format.
 @param    filename The filename of the Bitmap (BMP) to be loaded.
 @return   A structure containing the image data.
 */
static TImage loadBMPGraphicFile(std::string &filename)
{
    BIPHeader bip_header;
    TImage image = {0};
    
    std::ifstream infile;
    
    infile.open(filename, std::ios::in | std::ios::binary);
    if (!infile.is_open()) {
        return image;
    }
    
    infile.read((char *)&bip_header, sizeof(BIPHeader));

    if (strncmp(bip_header.fileHeader.bfType, "BM", 2) != 0) {
        infile.close();
        return image;
    }
    
    image.bitWidth = 8;
    image.width = abs(bip_header.biWidth);
    image.height = abs(bip_header.biHeight);
    
    image.data = new uint8_t[image.width * image.height];
    if (image.data == nullptr) {
        infile.close();
        std::cout << "Memory not allocated for image data.\n";
        return image;
    }
    
    memset(image.data, 0, image.width * image.height);
    
    // Load palette data only if they is palette data to be loaded.
    if (bip_header.biClrUsed) {
        image.colors = bip_header.biClrUsed;
        image.palt = new uint32_t[bip_header.biClrUsed];
        
        if (image.palt == nullptr) {
            infile.close();
            if (image.data != nullptr) delete [] image.data;
            std::cout << "Failed to allocated memory for palette data.\n";
            return image;
        }
        
        
        infile.read((char *)image.palt, bip_header.biClrUsed * sizeof(uint32_t));
        for (int i = 0; i < bip_header.biClrUsed; ++i) {
#ifdef __LITTLE_ENDIAN__
            image.palt[i] = swap_endian(image.palt[i]);
#endif
            image.palt[i] >>= 8; // Convert ARGB to RGBA, (alpha channel will always be zero)
            image.palt[i] |= 0xFF000000;
        }
    }
    
    size_t length = image.width / (8 / bip_header.biBitCount);
    uint8_t *ptr = image.data;
    
    // Read in the pixels, line by line
    for (int r = 0; r < image.height; ++r, ptr+=image.width) {
        infile.read((char *)ptr, length);
        if (infile.gcount() != length) {
            std::cout << filename << " Read failed!\n";
            break;
        }
        
        // Deal with any padding if nessasary.
        if (length % 6)
            infile.seekg(length % 6, std::ios_base::cur);
        
        if (bip_header.biBitCount == 4)
            unpack4BitValues(ptr, ptr, length);
    }
    
    infile.close();
    
    if (bip_header.biHeight > 0)
        flipImageVertically(image);
    
    return image;
}

/**
 @brief    Loads a file in the Portable Bitmap (PBM) format.
 @param    filename The filename of the Portable Bitmap (PBM) to be loaded.
 @return   A structure containing the image data.
 */
static TImage loadPBMGraphicFile(std::string &filename)
{
    std::ifstream infile;
    
    TImage image = {0};
  
    
    infile.open(filename, std::ios::in | std::ios::binary);
    if (!infile.is_open()) {
        return image;
    }
    
    std::string s;
    
    getline(infile, s);
    if (s != "P4") {
        infile.close();
        return image;
    }
    
    image.bitWidth = 1;
    
    getline(infile, s);
    image.width = atoi(s.c_str());
    
    getline(infile, s);
    image.height = atoi(s.c_str());
    
    size_t length = ((image.width + 7) >> 3) * image.height;
    image.data = new uint8_t[length];
    
    if (!image.data) {
        infile.close();
        return image;
    }
    infile.read((char *)image.data, length);
    
    infile.close();
    return image;
}

TImage loadImage(std::string &filename)
{
    if (isBMP(filename)) return loadBMPGraphicFile(filename);
    if (isPBM(filename)) return loadPBMGraphicFile(filename);
    return {0};
}

TImage createBitmap(int w, int h)
{
    TImage image = {0};
    
    w = (w + 7) & ~7;
    image.data = new uint8_t[w * h / 8];
    if (!image.data) {
        return image;
    }
    
    image.bitWidth = 1;
    image.width = w;
    image.height = h;
    
    return image;
}

TImage createPixmap(int w, int h)
{
    TImage image = {0};
   
    
    image.data = new uint8_t[w * h];
    if (!image.data) {
        return image;
    }
    
    image.bitWidth = 8;
    image.width = w;
    image.height = h;
    
    return image;
}

void copyPixmap(const TImage& dst, int dx, int dy, const TImage& src, int x, int y, uint16_t w, uint16_t h)
{
    if (!dst.data || !src.data)
        return;
    
    uint8_t *d = (uint8_t *)dst.data;
    uint8_t *s = (uint8_t *)src.data;
    
    d += dx + dy * (int)dst.width;
    s += x + y * (int)src.width;
    
    for (int j=0; j<h; j++) {
        for (int i=0; i<w; i++) {
            dst.data[i + dx + (j + dy) * dst.width] = src.data[i + x + (j + y) * src.width];
        }
        d += dst.width;
        s += src.width;
    }
}

TImage convertMonochromeBitmapToPixmap(const TImage &monochrome)
{
    TImage image = {0};
   
    
    uint8_t *src = (uint8_t *)monochrome.data;
    uint8_t bitPosition = 1 << 7;
    
    image.bitWidth = 8;
    image.width = monochrome.width;
    image.height = monochrome.height;
    image.data = new uint8_t[image.width * image.height];
    if (!image.data) return image;
    
    memset(image.data, 0, image.width * image.height);
    
    uint8_t *dest = (uint8_t *)image.data;
    
    int x, y;
    for (y=0; y<monochrome.height; y++) {
        bitPosition = 1 << 7;
        for (x=0; x<monochrome.width; x++) {
            *dest++ = (*src & bitPosition ? 1 : 0);
            if (bitPosition == 1) {
                src++;
                bitPosition = 1 << 7;
            } else {
                bitPosition >>= 1;
            }
        }
        if (x & 7) src++;
    }
    
    return image;
}

void reset(TImage& image)
{
    if (image.data != nullptr) delete[] image.data;
    if (image.palt != nullptr) delete[] image.palt;
    image = {0};
}

bool containsImage(const TImage &image, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    if (!image.data) return false;
    if (x + w > image.width || y + h > image.height) return false;
    uint8_t *p = (uint8_t *)image.data;
    
    p += x + y * image.width;
    while (h--) {
        for (int i=0; i<w; i++) {
            if (p[i]) return true;
        }
        p+=image.width;
    }
    return false;
}

TImage extractImageSection(TImage& image)
{
    TImage extractedImage = {0};
    extractedImage = extractImageSectionMasked(image, 0);
    return extractedImage;
}

TImage extractImageSectionMasked(TImage &image, uint8_t maskColor)
{
    TImage extractedImage = {0};
    
    int minX, maxX, minY, maxY;
    
    if (!image.data) return image;
    
    uint8_t *p = (uint8_t *)image.data;
    
    maxX = 0;
    maxY = 0;
    minX = image.width - 1;
    minY = image.height - 1;
    
    for (int y=0; y<image.height; y++) {
        for (int x=0; x<image.width; x++) {
            if (p[x + y * image.width] == maskColor) continue;
            if (minX > x) minX = x;
            if (maxX < x) maxX = x;
            if (minY > y) minY = y;
            if (maxY < y) maxY = y;
        }
    }
    
    if (maxX < minX || maxY < minY)
        return image;
    
    
    int width = maxX - minX + 1;
    int height = maxY - minY + 1;
    
    extractedImage = createPixmap(width, height);
    if (!extractedImage.data) return image;
    copyPixmap(extractedImage, 0, 0, image, minX, minY, width, height);
    
    return extractedImage;
}
