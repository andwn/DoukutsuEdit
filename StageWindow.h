#ifndef STAGE9_STAGEWINDOW_H
#define STAGE9_STAGEWINDOW_H

#define PXA_MAX 256
#define TSC_MAX 0x8000

enum {
    EDIT_PENCIL, EDIT_ERASER, EDIT_ENTITY
};

#include "pxm.h"
#include "PXE.h"

extern const char *vertex_src;
extern const char *fragment_src;
extern uint32_t CompileShader(int type, const char *source);

class StageWindow {
public:
    StageWindow();
    ~StageWindow();
    void Render();
private:
    uint32_t white_tex;
    void SetDefaultFB();
    void DrawRect(float x, float y, float w, float h, uint32_t c);
    void DrawRectEx(float x, float y, float w, float h, float tx, float ty, float tw, float th, uint32_t c);
    void DrawGrid(int xx, int yy);

    // Toolbar
    int editMode;
    //bool showAttributes;
    //bool showEvents;

    // Map
    std::string pxm_fname;
    std::string pxe_fname;
    std::string tsc_fname;
    PXM pxm;
    PXE pxe;
    char tsc_text[TSC_MAX];
    uint32_t map_fb, map_tex;
    float map_zoom;
    uint16_t lastMapW, lastMapH;
    int selectedEntity;
    uint16_t clickingMapX, clickingMapY;
    bool tsc_obfuscated;
    void CreateMapFB(int w, int h);
    void FreeMapFB();
    void SetMapFB();
    void OpenMap(std::string fname);
    void SaveMap();

    // Tileset
    std::string tileset_fname;
    std::string pxa_fname;
    //uint32_t tileset;
    uint32_t tileset_fb, tileset_tex;
    uint32_t tileset_image;
    int tileset_width, tileset_height;
    uint8_t pxa[PXA_MAX];
    uint16_t clickingTile, selectedTile;
    void CreateTilesetFB();
    void FreeTilesetFB();
    void SetTilesetFB();
    void OpenTileset(std::string fname);
    void SaveTileset();

    // Status Bar

    // Shader and VAO
    uint32_t vao;
    uint32_t vbo;
    uint32_t program;
    uint32_t vertex_id;
    uint32_t fragment_id;
    int32_t uf_scale;
    int32_t uf_offset;
    uint32_t attr_pos;
    uint32_t attr_uv;
    uint32_t attr_color;

    uint32_t CompileShader(int type, const char *source);
    void InitShaders();
    void FreeShaders();

};

#endif //STAGE9_STAGEWINDOW_H
