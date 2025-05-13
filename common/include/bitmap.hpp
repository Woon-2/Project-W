#ifndef __Bitmap_HPP
#define __Bitmap_HPP

#include "stdafx.hpp"

#include "FreeImage.h"

class Bitmap {
public:
    Bitmap();
    Bitmap(const std::filesystem::path& path);
    ~Bitmap();

    void load(const std::filesystem::path& path);
    void unload();

    std::optional<RGBQUAD> getPixel(int x, int y) const;

    FIBITMAP* getBitmap() const { return bitmap_; }
    BYTE* getBits() const { return bits_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    FIBITMAP* bitmap_;
    BYTE* bits_;
    int width_;
    int height_;
};

#endif // __Bitmap_HPP