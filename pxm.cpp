#include "common.h"
#include "pxm.h"

void PXM::Resize(uint16_t _width, uint16_t _height) {
    uint8_t *temp = (uint8_t*) calloc(_width * _height, 1);
    for(uint16_t y = 0; y < _height && y < height; y++) {
        memcpy(&temp[y * _width], &tiles[y * width], min(width, _width));
    }
    free(tiles);
    width = _width;
    height = _height;
    tiles = temp;
}

void PXM::SetTile(uint16_t x, uint16_t y, uint8_t tile) {
    if(x < width && y < height) tiles[y * width + x] = tile;
}

void PXM::Clear() {
    memset(tiles, 0, width * height);
}

void PXM::Load(FILE *file) {
    char head[4];
    fread(head, 1, 4, file);
    fread(&width, 2, 1, file);
    fread(&height, 2, 1, file);
    free(tiles);
    tiles = (uint8_t*) calloc(width * height, 1);
    fread(tiles, 1, width * height, file);
}

void PXM::Save(FILE *file) {
    static const char head[4] = "PXM";
    fwrite(head, 1, 4, file);
    fwrite(&width, 2, 1, file);
    fwrite(&height, 2, 1, file);
    fwrite(tiles, 1, width * height, file);
}
