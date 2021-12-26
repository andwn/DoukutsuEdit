#pragma once

class PXM {
public:
    PXM() : PXM(20, 15) {}
    PXM(uint16_t _width, uint16_t _height) : width(_width), height(_height) {
        tiles = (uint8_t*) calloc(width * height, 1);
    }
    ~PXM() { free(tiles); }
    uint16_t Width() const { return width; }
    uint16_t Height() const { return height; }
    uint8_t Tile(uint16_t x, uint16_t y) {
        return x < width ? y < height ? tiles[x + y * width] : 0 : 0;
    }

    void Resize(uint16_t _width, uint16_t _height);
    void SetTile(uint16_t x, uint16_t y, uint8_t tile);
    void Clear();
    //void Shift(int16_t x, int16_t y);
    void Load(FILE *file);
    void Save(FILE *file);
private:
    uint16_t width, height;
    uint8_t *tiles;
};
