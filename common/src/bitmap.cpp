#include "bitmap.hpp"

Bitmap::Bitmap()
    : bitmap_(nullptr), bits_(nullptr), width_(0), height_(0) {}

Bitmap::Bitmap(const std::filesystem::path& path)
    : bitmap_(nullptr), bits_(nullptr), width_(0), height_(0) {
    load(path);
}

Bitmap::~Bitmap() {
    unload();
}

void Bitmap::load(const std::filesystem::path& path) {
    unload();

    bitmap_ = FreeImage_Load(FIF_PNG, path.string().c_str(), PNG_DEFAULT);
    if (!bitmap_) {
        throw std::runtime_error("Failed to load bitmap: " + path.string());
    }
    bits_ = FreeImage_GetBits(bitmap_);
    if (!bits_) {
        FreeImage_Unload(bitmap_);
        throw std::runtime_error("Failed to get bitmap bits: " + path.string());
    }
    width_ = FreeImage_GetWidth(bitmap_);
    height_ = FreeImage_GetHeight(bitmap_);

    if (width_ == 0 || height_ == 0) {
        FreeImage_Unload(bitmap_);
        throw std::runtime_error("Invalid bitmap dimensions: " + path.string());
    }
}

std::optional<RGBQUAD> Bitmap::getPixel(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return std::nullopt;
    }

    RGBQUAD pixel;
    FreeImage_GetPixelColor(bitmap_, x, y, &pixel);
    return pixel;
}

void Bitmap::unload() {
    if (bitmap_) {
        FreeImage_Unload(bitmap_);
        bitmap_ = nullptr;
        bits_ = nullptr;
        width_ = 0;
        height_ = 0;
    }
}