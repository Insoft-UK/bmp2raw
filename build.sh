g++ -std=c++20 src/*.cpp -o bin/arm/bmp2raw -Os -fno-ident -fno-asynchronous-unwind-tables -target arm64-apple-macos11
g++ -std=c++20 src/*.cpp -o bin/bmp2raw -Os -fno-ident -fno-asynchronous-unwind-tables -target x86_64-apple-macos10.12
strip bin/bmp2raw
strip bin/arm/bmp2raw

IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | awk '{print $2}')
codesign -s "$IDENTITY" ./bin/bmp2raw

IDENTITY=$(security find-identity -v -p codesigning | grep "Developer ID Application" | awk '{print $2}')
codesign -s "$IDENTITY" ./bin/arm/bmp2raw

