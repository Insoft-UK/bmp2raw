/*
 Copyright (C) 2014 by Insoft
 
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

#include <iostream>
#include <sstream>
#include <vector>
#include <regex>
#include <fstream>
#include <iomanip>

#include "build.h"
#include "image.hpp"

bool verbose = false;

void usage(void)
{
    std::cout << "Copyright (C) 2024 Insoft. All rights reserved.\n";
    std::cout << "Bitmap RGB565 Raw Image Creator.\n\n";
    std::cout << "Usage: bmp2raw infile [-options]\n";
    std::cout << "\n";
    
    // TODO: Add verbose
    std::cout << " -o outfile\n";
    std::cout << "\n";
    std::cout << "Usage: bmp2raw {-version | -help}\n";
}

void error(void)
{
    std::cout << "bmp2raw: try 'bmp2raw -help' for more information\n";
    exit(0);
}

void version(void) {
    std::cout
    << "Version: bmp2raw "
    << (unsigned)__BUILD_NUMBER / 100000 << "."
    << (unsigned)__BUILD_NUMBER / 10000 % 10 << "."
    << (unsigned)__BUILD_NUMBER / 1000 % 10 << "."
    << std::setfill('0') << std::setw(3) << (unsigned)__BUILD_NUMBER % 1000
    << "\n";
    std::cout << "Copyright: (C) 2024 Insoft. All rights reserved.\n";
}

// Define a structure for RGB888 using bit-fields
struct RGBA8888 {
#ifdef __LITTLE_ENDIAN__
    uint32_t r : 8;
    uint32_t g : 8;
    uint32_t b : 8;
    uint32_t a : 8;
#else
    uint32_t a : 8;
    uint32_t b : 8;
    uint32_t g : 8;
    uint32_t r : 8;
#endif
};

// Define a structure for RGB565 using bit-fields
struct RGB565 {
    uint16_t b : 5;
    uint16_t g : 6;
    uint16_t r : 5;
};

// Function to convert RGB888 to RGB565
uint16_t convertRGBA8888ToRGB565(uint32_t rgba8888Value) {
    // Interpret the 32-bit integer as an RGB888 structure
    RGBA8888 *rgba8888 = reinterpret_cast<RGBA8888 *>(&rgba8888Value);

    // Create an RGB565 structure and populate its fields
    RGB565 rgb565;
    rgb565.r = rgba8888->r >> 3; // Convert 8-bit red to 5-bit
    rgb565.g = rgba8888->g >> 2; // Convert 8-bit green to 6-bit
    rgb565.b = rgba8888->b >> 3; // Convert 8-bit blue to 5-bit

    // Combine the bit-fields into a single 16-bit value
    uint16_t* rgb565Value = reinterpret_cast<uint16_t*>(&rgb565);
#ifdef __LITTLE_ENDIAN__
    *rgb565Value = *rgb565Value >> 8 | *rgb565Value << 8;
#endif
    return *rgb565Value;
}

int main(int argc, const char * argv[])
{
    if ( argc == 1 ) {
        error();
        return 0;
    }
    
    std::string in_filename, out_filename;
    
    for( int n = 1; n < argc; n++ ) {
        if (*argv[n] == '-') {
            std::string args(argv[n]);
            
            if (args == "-o") {
                if (++n > argc) error();
                out_filename = argv[n];
                continue;
            }
            
            if (args == "-version") {
                version();
                return 0;
            }
            
            if (args == "-v") {
                if (++n > argc) error();
                args = argv[n];
                verbose = true;
                continue;
            }
            
            error();
            return 0;
        }
        in_filename = argv[n];
    }
    
    if (out_filename.empty()) {
        size_t pos = in_filename.rfind(".");
        if (pos != std::string::npos) {
            out_filename = in_filename.substr(0, pos) + ".raw";
        } else {
            out_filename = in_filename + ".raw";
        }
    }
    
    TImage image = loadImage(in_filename);
    if (image.data == nullptr) return 0;
    

    uint16_t palt[256];
    for (int i = 0; i < image.colors; ++i) {
        palt[i] = convertRGBA8888ToRGB565(image.palt[i]);
    }
    
    std::ofstream out_file;
    out_file.open(out_filename, std::ios::out | std::ios::binary);

    if (!out_file.is_open()) {
        reset(image);
        return 0;
    }

    
    uint8_t* pixel = image.data;
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            out_file.write((const char *)&palt[*pixel], sizeof(uint16_t));
            pixel++;
        }
    }
    
    out_file.close();

    reset(image);
    return 0;
}
