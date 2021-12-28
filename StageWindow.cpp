#include "common.h"
#include <filesystem>

#include "imgui/imgui.h"
#include "imgui/ImGuiFileDialog.h"
#define STB_IMAGE_IMPLEMENTATION
#include "imgui/stb_image.h"
#include "glad.h"

#include "Preferences.h"
#include "StageWindow.h"

static void chkerr(int line) {
#ifdef DEBUG
    for(int i = glGetError(); i != GL_NO_ERROR; i = glGetError()) {
		switch(i) {
			case GL_INVALID_ENUM: printf("%d: GL_INVALID_ENUM\n", line); break;
			case GL_INVALID_VALUE: printf("%d: GL_INVALID_VALUE\n", line); break;
			case GL_INVALID_OPERATION: printf("%d: GL_INVALID_OPERATION\n", line); break;
			case GL_OUT_OF_MEMORY: printf("%d: GL_OUT_OF_MEMORY\n", line); break;
			default: printf("%d: Unknown OpenGL Error: '%d'\n", line, i); break;
		}
	}
#endif
}

const char *vertex_src =
#if defined(IMGUI_IMPL_OPENGL_ES2)
        "#version 100\n"
#elif defined(__APPLE__)
        "#version 150\n"
#else
        "#version 130\n"
#endif
        "in vec2 position;\n"
        "in vec2 texcoord;\n"
        "in vec4 color;\n"
        "uniform vec2 scale;\n"
        "uniform vec2 offset;\n"
        "out vec2 frag_texcoord;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "    frag_texcoord = texcoord;\n"
        "    frag_color = color;\n"
        "    gl_Position = vec4(position * scale + offset, 1.0, 1.0);\n"
        "}\n";
const char *fragment_src =
#if defined(IMGUI_IMPL_OPENGL_ES2)
        "#version 100\n"
#elif defined(__APPLE__)
        "#version 150\n"
#else
        "#version 130\n"
#endif
        "in vec2 frag_texcoord;\n"
        "in vec4 frag_color;\n"
        "uniform sampler2D tex;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 texcolor = texture(tex, frag_texcoord) * frag_color;\n"
        "    if(texcolor.a < 0.01) discard;\n"
        "    fragColor = texcolor;\n"
        "}\n";

uint32_t StageWindow::CompileShader(int type, const char *source) {
    if(glCreateShader == NULL || glGenFramebuffers == NULL) {
        printf("Your graphics driver does not support OpenGL 3.2.\n");
        exit(1);
    }
    uint32_t id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);

    int success;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (success != GL_TRUE) {
        int logLength;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char*) malloc(logLength);
        glGetShaderInfoLog(id, logLength, &logLength, log);
        printf("Failed to compile %s shader:\n%s\n",
              type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        free(log);
        exit(1);
    }
    return id;
}
void StageWindow::InitShaders() {
    vertex_id = CompileShader(GL_VERTEX_SHADER, vertex_src);
    fragment_id = CompileShader(GL_FRAGMENT_SHADER, fragment_src);
    program = glCreateProgram();
    glAttachShader(program, vertex_id);
    glAttachShader(program, fragment_id);
    glLinkProgram(program);
    glDetachShader(program, vertex_id);
    glDetachShader(program, fragment_id);
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        int logLength;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        char *log = (char*) malloc(logLength);
        glGetProgramInfoLog(program, logLength, &logLength, log);
        printf("Failed to compile shader program:\n%s\n", log);
        free(log);
        exit(1);
    }
    glUseProgram(program);
    chkerr(__LINE__);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    chkerr(__LINE__);
    uf_scale = glGetUniformLocation(program, "scale");
    uf_offset = glGetUniformLocation(program, "offset");
    glUniform2f(uf_scale, 1, 1);
    glUniform2f(uf_offset, 0, 0);
    chkerr(__LINE__);
    // C isn't for ImDrawVert, but it's still good enough for me
    attr_pos = glGetAttribLocation(program, "position");
    attr_uv = glGetAttribLocation(program, "texcoord");
    attr_color = glGetAttribLocation(program, "color");
    glEnableVertexAttribArray(attr_pos);
    glEnableVertexAttribArray(attr_uv);
    glEnableVertexAttribArray(attr_color);
    glVertexAttribPointer(attr_pos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(attr_uv,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));
    chkerr(__LINE__);
}
void StageWindow::FreeShaders() {
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glUseProgram(0);
    glDeleteProgram(program);
    chkerr(__LINE__);
}

StageWindow::StageWindow() {
    pxm_fname = "untitled.pxm";
    pxe_fname = "untitled.pxe";
    tsc_fname = "untitled.tsc";
    tileset_fname = "untitled.png";
    pxa_fname = "untitled.pxa";
    //npc_fname = "";
    lastMapW = lastMapH = 0;
    map_fb = tileset_fb = 0;
    tileset_image = 0;
    tileset_width = tileset_height = 0;
    selectedEntity = -1;
    selectedTile = 0;
    tileRange[0] = tileRange[1] = 0;
    tileRange[2] = tileRange[3] = 1;
    tsc_text[0] = 0;
    tsc_obfuscated = false;
    CreateTilesetFB();
    // Blank white texture
    glGenTextures(1, &white_tex);
    glBindTexture(GL_TEXTURE_2D, white_tex);
    static const uint32_t white[4] = { 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    // Checkerboard texture
    glGenTextures(1, &back_tex);
    glBindTexture(GL_TEXTURE_2D, back_tex);
    static const uint32_t back[4] = { 0xFF999999,0xFFBBBBBB,0xFFBBBBBB,0xFF999999 };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
    chkerr(__LINE__);
    InitShaders();

    Preferences &pref = Preferences::Instance();
    if(pref.recentPXM[0].length() > 0) OpenMap(pref.recentPXM[0]);
    if(pref.recentTS[0].length() > 0) OpenTileset(pref.recentTS[0]);
    if(pref.npcListPath.length() > 0) LoadNpcList(pref.npcListPath);
}

StageWindow::~StageWindow() {
    glDeleteTextures(1, &white_tex);
    FreeMapFB();
    FreeTilesetFB();
    FreeShaders();
}

void StageWindow::CreateMapFB(int w, int h) {
    FreeMapFB();
    // Frame Buffer
    glGenFramebuffers(1, &map_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, map_fb);
    // Target Texture
    glGenTextures(1, &map_tex);
    glBindTexture(GL_TEXTURE_2D, map_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, map_tex, 0);
    chkerr(__LINE__);
    SetDefaultFB();
}

void StageWindow::CreateTilesetFB() {
    FreeTilesetFB();
    // Frame Buffer
    glGenFramebuffers(1, &tileset_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, tileset_fb);
    // Target Texture
    glGenTextures(1, &tileset_tex);
    glBindTexture(GL_TEXTURE_2D, tileset_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tileset_tex, 0);
    chkerr(__LINE__);
    SetDefaultFB();
}

void StageWindow::FreeMapFB() {
    if(map_fb) {
        glDeleteTextures(1, &map_tex);
        glDeleteFramebuffers(1, &map_fb);
        map_tex = 0;
        map_fb = 0;
    }
}

void StageWindow::FreeTilesetFB() {
    if(tileset_fb) {
        glDeleteTextures(1, &tileset_tex);
        glDeleteFramebuffers(1, &tileset_fb);
        tileset_tex = 0;
        tileset_fb = 0;
    }
}

void StageWindow::SetDefaultFB() {
    glBindFramebuffer(GL_FRAMEBUFFER, NULL);
}

void StageWindow::SetMapFB() const {
    glBindFramebuffer(GL_FRAMEBUFFER, map_fb);
}

void StageWindow::SetTilesetFB() const {
    glBindFramebuffer(GL_FRAMEBUFFER, tileset_fb);
}

void StageWindow::DrawRect(float x, float y, float w, float h, uint32_t c) {
    DrawRectEx(x, y, w, h, 0, 0, 1, 1, c);
}

void StageWindow::DrawRectEx(float x, float y, float w, float h, float tx, float ty, float tw, float th, uint32_t c) {
    ImDrawVert vtx[4] = {
            { ImVec2(x,   y  ), ImVec2(tx,    ty   ), ImColor(c) },
            { ImVec2(x,   y+h), ImVec2(tx,    ty+th), ImColor(c) },
            { ImVec2(x+w, y  ), ImVec2(tx+tw, ty   ), ImColor(c) },
            { ImVec2(x+w, y+h), ImVec2(tx+tw, ty+th), ImColor(c) },
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(ImDrawVert) * 4, vtx, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    chkerr(__LINE__);
}

void StageWindow::DrawUnfilledRect(float x, float y, float w, float h, uint32_t color) {
    ImColor c = color;
    ImVec2 t = ImVec2(0,0);
    ImVec2 v[4] = { ImVec2(x, y), ImVec2(x, y+h), ImVec2(x+w, y), ImVec2(x+w, y+h) };
    ImDrawVert vtx[6*4] = {
            { v[0], t, c }, { v[1], t, c }, { v[0], t, c }, { v[1], t, c }, { v[1], t, c }, { v[0], t, c }, // L
            { v[0], t, c }, { v[0], t, c }, { v[2], t, c }, { v[0], t, c }, { v[2], t, c }, { v[2], t, c }, // U
            { v[2], t, c }, { v[3], t, c }, { v[2], t, c }, { v[3], t, c }, { v[3], t, c }, { v[2], t, c }, // R
            { v[1], t, c }, { v[1], t, c }, { v[3], t, c }, { v[1], t, c }, { v[3], t, c }, { v[3], t, c }, // D
    };
    vtx[6*0+2].pos.x++; vtx[6*0+4].pos.x++; vtx[6*0+5].pos.x++; // L
    vtx[6*1+1].pos.y++; vtx[6*1+3].pos.y++; vtx[6*1+4].pos.y++; // U
    vtx[6*2+0].pos.x--; vtx[6*2+1].pos.x--; vtx[6*2+3].pos.x--; // R
    vtx[6*3+0].pos.y--; vtx[6*3+2].pos.y--; vtx[6*3+5].pos.y--; // D
    glBufferData(GL_ARRAY_BUFFER, sizeof(ImDrawVert) * 6*4, vtx, GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6*4);
    chkerr(__LINE__);
}

void StageWindow::DrawBack(int xx, int yy) {
    glBindTexture(GL_TEXTURE_2D, back_tex);
    auto c = ImColor(0xFFFFFFFF);
    ImVec2 uv[4] = { ImVec2(0,0), ImVec2(0,1), ImVec2(1,0), ImVec2(1,1) };
    for(int y = 0; y < yy; y++) {
        std::vector<ImDrawVert> vtx;
        for(int x = 0; x < xx; x++) {
            ImDrawVert v[6];
            v[0] = { ImVec2(x*16,    y*16),    uv[0], c };
            v[1] = { ImVec2(x*16,    y*16+16), uv[1], c };
            v[2] = { ImVec2(x*16+16, y*16),    uv[2], c };
            v[3] = v[1];
            v[4] = { ImVec2(x*16+16, y*16+16), uv[3], c };
            v[5] = v[2];
            for(auto & i : v) vtx.emplace_back(i);
        }
        glBufferData(GL_ARRAY_BUFFER, sizeof(ImDrawVert) * vtx.size(), vtx.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, vtx.size());
    }
}

void StageWindow::DrawGrid(int xx, int yy) {
    // Draw grid
    glBindTexture(GL_TEXTURE_2D, white_tex);
    for(int x = 0; x < xx; x++) {
        DrawRect(float(x) * 16, 0, 1, float(yy) * 16, 0xAAFFFFFF);
    }
    for(int y = 0; y < yy; y++) {
        DrawRect(0, float(y) * 16, float(xx) * 16, 1, 0xAAFFFFFF);
    }
}

uint32_t StageWindow::LoadTexture(const char *fname, int *w, int *h, bool transparent) {
    uint32_t texid = 0;
    FILE *file = fopen(fname, "rb");
    if(file) {
        stbi_uc *rgba_data = stbi_load_from_file(file, w, h, NULL, STBI_rgb_alpha);
        if(rgba_data) {
            if(transparent) {
                auto pixels = (uint32_t*) rgba_data;
                uint32_t tcolor = pixels[0];
                for(int i = 0; i < *w * *h; i++) if(pixels[i] == tcolor) pixels[i] = 0;
            }
            glGenTextures(1, &texid);
            glBindTexture(GL_TEXTURE_2D, texid);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
            chkerr(__LINE__);
            stbi_image_free(rgba_data);
        }
        fclose(file);
    }
    return texid;
}

void StageWindow::FreeTexture(uint32_t tex) {
    glDeleteTextures(1, &tex);
}

void StageWindow::OpenMap(std::string fname) {
    FILE *file = fopen(fname.c_str(), "rb");
    if(file) {
        pxm_fname = fname;
        pxm.Load(file);
        fclose(file);
    }
    // Try to open PXE with the same base name
    selectedEntity = -1;
    std::string newfn = fname.substr(0, fname.find_last_of('.')) + ".pxe";
    file = fopen(newfn.c_str(), "rb");
    if(file) {
        pxe_fname = newfn;
        pxe.Load(file);
        fclose(file);
        Preferences::Instance().AddRecentPXM(pxm_fname);
    }
    // Try to open TSC/TXT with the same base name in ./ or ../tsc
    tsc_text[0] = 0;
    tsc_obfuscated = false;
    std::string tscfn = fname.substr(0, fname.find_last_of('.')) + ".tsc";
    std::string txtfn = fname.substr(0, fname.find_last_of('.')) + ".txt";
    file = fopen(tscfn.c_str(), "rb");
    if(file) tsc_fname = tscfn;
    else file = fopen(txtfn.c_str(), "rb");
    if(file) tsc_fname = txtfn;
    else {
        size_t dirpos = tscfn.find("/Stage/");
        if(dirpos != std::string::npos) {
            std::string tscfn2 = tscfn.replace(dirpos, 7, "/tsc/");
            std::string txtfn2 = txtfn.replace(dirpos, 7, "/tsc/");
            file = fopen(tscfn2.c_str(), "rb");
            if(file) tsc_fname = tscfn2;
            else file = fopen(txtfn2.c_str(), "rb");
            if(file) tsc_fname = txtfn2;
        }
    }
    if(file) {
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        if(size > TSC_MAX-1) size = TSC_MAX-1;
        fseek(file, 0, SEEK_SET);
        fread(tsc_text, 1, size, file);
        tsc_text[size] = 0;
        // The <END command should be in every script, so if it's not found
        // assume the TSC is obfuscated
        if(std::string(tsc_text).find("<END") == std::string::npos) {
            tsc_obfuscated = true;
            char key = tsc_text[size / 2];
            for(size_t i = 0; i < size; i++) {
                if(tsc_text[i] != key) tsc_text[i] -= key;
            }
        }
        fclose(file);
    }
}

void StageWindow::SaveMap() {
    FILE *file = fopen(pxm_fname.c_str(), "wb");
    if(file) {
        pxm.Save(file);
        fclose(file);
    }
    file = fopen(pxe_fname.c_str(), "wb");
    if(file) {
        pxe.Save(file);
        fclose(file);
    }
}

void StageWindow::SaveScript() {
    FILE *file = fopen(tsc_fname.c_str(), "wb");
    if(file) {
        char data[TSC_MAX];
        size_t len = strnlen(tsc_text, TSC_MAX);
        if(tsc_obfuscated) {
            char key = tsc_text[len / 2];
            int i = 0;
            while(tsc_text[i] != 0) {
                data[i] = tsc_text[i] == key ? tsc_text[i] : tsc_text[i] + key;
                i++;
            }
        } else {
            strncpy(data, tsc_text, TSC_MAX);
        }
        fwrite(data, 1, len, file);
        fclose(file);
    }
}

void StageWindow::OpenTileset(std::string fname) {
    tileset_fname = "";
    if(tileset_image) FreeTexture(tileset_image);
    tileset_image = LoadTexture(fname.c_str(), &tileset_width, &tileset_height, true);
    if(tileset_image) {
        tileset_fname = fname;
        tileset_width /= 16;
        tileset_height /= 16;
        Preferences::Instance().AddRecentTS(tileset_fname);
    }
    // Try to open PXA with the same base name
    selectedTile = 0;
    tileRange[0] = tileRange[1] = 0;
    tileRange[2] = tileRange[3] = 1;
    std::string newfn = fname.substr(0, fname.find_last_of('.')) + ".pxa";
    size_t prt_loc = newfn.find("Prt");
    if(prt_loc != std::string::npos) newfn = newfn.erase(prt_loc, 3);
    //printf("%s\n", newfn.c_str());
    FILE *file = fopen(newfn.c_str(), "rb");
    if(file) {
        pxa_fname = newfn;
        fread(pxa, 1, min(tileset_width * tileset_height, PXA_MAX), file);
        fclose(file);
    }
}

void StageWindow::SaveTileset() {
    FILE *file = fopen(pxa_fname.c_str(), "wb");
    if(file) {
        fwrite(pxa, 1, tileset_width * tileset_height, file);
        fclose(file);
    }
}

static std::string SlurpFile(std::filesystem::path path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if(!f.is_open()) return {};
    const auto sz = std::filesystem::file_size(path);
    std::string result(sz, '\0');
    f.read(result.data(), sz);
    return result;
}

void StageWindow::LoadNpcList(std::string fname) {
    FILE *file = fopen(fname.c_str(), "r");
    if(file) {
        size_t pathPos = fname.find("/src/db/npc.c");
        std::string sheet_str = SlurpFile(fname.substr(0, pathPos) + "/src/sheet.c");
        std::string res_str = SlurpFile(fname.substr(0, pathPos) + "/res/resources.res");
        for(auto & s : npc_sprites) {
            if(s.texture) FreeTexture(s.texture);
        }
        npc_sprites.clear();
        char buf[256];
        while(fgets(buf, 256, file) != NULL) {
            // FIXME: This assumes all NPC definitions are a single line
            char *start = strchr(buf, '{');
            if(start == NULL) continue;
            char *end = strchr(buf, '}');
            if(end == NULL) continue;
            // This line contains an NPC, check if there is a sprite pointer or sheet
            char *spr = strstr(start, "&SPR_");
            char *sht = strstr(start, "SHEET_");
            if(sht && !spr) {
                // Have to look in sheet.c to find the sprite variable
                char *sht_end = strstr(sht, ",");
                size_t len = sht_end - sht;
                if(len < 64) {
                    char sht_var[64];
                    memcpy(sht_var, sht, len);
                    sht_var[len] = 0;
                    //printf("%s\n", sht_var);
                    size_t shtpos = sheet_str.find(sht_var);
                    if(shtpos != std::string::npos) {
                        // Pull SPR var from ADD_SHEET line
                        std::string line = sheet_str.substr(shtpos);
                        line = line.substr(0, line.find_first_of("\n\0"));
                        strncpy(buf, line.c_str(), 256);
                        buf[min(line.length(),255)] = 0;
                        //printf("%s\n", buf);
                        spr = strstr(buf, "&SPR_");
                    }
                }
            }
            if(spr) {
                // Sprite variable found, get filename and dimensions from resources.res
                char *spr_end = strstr(spr, ",");
                size_t len = spr_end - spr - 1;
                if(len < 64) {
                    char spr_var[64];
                    memcpy(spr_var, spr + 1, len);
                    spr_var[len] = 0;
                    size_t respos = res_str.find(spr_var);
                    if(respos != std::string::npos) {
                        // Pull filename from line containing SPR var
                        std::string line = res_str.substr(respos);
                        line = line.substr(0, line.find_first_of("\n\0"));
                        std::string spr_fname = line.substr(line.find('"') + 1);
                        spr_fname = spr_fname.substr(0, spr_fname.rfind('"'));
                        // Merge res relative path with full path
                        spr_fname.insert(0, fname.substr(0, pathPos) + "/res/");
                        if(!spr_fname.empty()) {
                            // Pull width and height
                            std::string size_str = trim_copy(line.substr(line.rfind('"') + 1));
                            int w = std::stoi(size_str, NULL);
                            int h = std::stoi(size_str.substr(size_str.find_first_of(" \t") + 1), NULL);
                            //printf("%s: %s - %d, %d\n", spr_var, spr_fname.c_str(), w, h);
                            if(w > 0 && h > 0) {
                                NpcSprite sprite;
                                sprite.texture = LoadTexture(spr_fname.c_str(), &sprite.tex_w, &sprite.tex_h, true);
                                if(sprite.texture) {
                                    sprite.fname = spr_fname;
                                    sprite.frame_w = w;
                                    sprite.frame_h = h;
                                    npc_sprites.emplace_back(sprite);
                                    continue;
                                }
                            }
                        }
                    }
                }
            }
            // No sprite or failed to find it, add a blank entry
            NpcSprite sprite;
            sprite.fname = "";
            sprite.texture = 0;
            npc_sprites.emplace_back(sprite);
        }
        Preferences::Instance().npcListPath = fname;
        Preferences::Instance().Save();
        fclose(file);
    }
}

bool StageWindow::Render() {
    ImGuiIO& io = ImGui::GetIO();
    Preferences &pref = Preferences::Instance();

    bool menuExit = false;
    bool popupNewMap = false, popupNewTileset = false, popupPreferences = false;
    ImGui::BeginMainMenuBar();
    {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Map")) {
                popupNewMap = true;
            }
            if (ImGui::MenuItem("Open Map...")) {
                ImGuiFileDialog::Instance()->OpenDialog("OpenMapFile", "Open Map File", ".pxm", ".");
            }
            if (ImGui::BeginMenu("Open Recent Map")) {
                for (auto & i : pref.recentPXM) {
                    if (i.empty()) break;
                    if (ImGui::MenuItem(i.c_str())) {
                        OpenMap(i);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear List")) {
                    for (auto & i : pref.recentPXM) i = "";
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Save Map")) {
                SaveMap();
            }
            if (ImGui::MenuItem("Save Map As...")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveMapFile", "Save Map File", ".pxm", ".");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Script")) {
                SaveScript();
            }
            if (ImGui::MenuItem("Save Script As...")) {
                ImGuiFileDialog::Instance()->OpenDialog("SaveScriptFile", "Save Script File", ".tsc,.txt", ".");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Tileset")) {
                popupNewTileset = true;
            }
            if (ImGui::MenuItem("Open Tileset")) {
                ImGuiFileDialog::Instance()->OpenDialog("OpenTilesetFile", "Open Tileset Image", ".bmp,.png", ".");
            }
            if (ImGui::BeginMenu("Open Recent Tileset")) {
                for(auto & i : pref.recentTS) {
                    if(i.empty()) break;
                    if(ImGui::MenuItem(i.c_str())) {
                        OpenTileset(i);
                    }
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Clear List")) {
                    for(auto & i : pref.recentTS) i = "";
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Save Tile Attributes")) {
                SaveTileset();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Open NPC List")) {
                ImGuiFileDialog::Instance()->OpenDialog("OpenNPCList", "Open NPC List", ".c", ".");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences")) {
                popupPreferences = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                menuExit = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            ImGui::Separator();
            ImGui::RadioButton("Insert Mode", &pref.editMode, EDIT_PENCIL);
            ImGui::RadioButton("Erase Mode", &pref.editMode, EDIT_ERASER);
            ImGui::RadioButton("Entity Mode", &pref.editMode, EDIT_ENTITY);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::Checkbox("Show Grid", &pref.showGrid);
            ImGui::Separator();
            ImGui::RadioButton("Zoom 100%", &pref.mapZoom, 1);
            ImGui::RadioButton("Zoom 200%", &pref.mapZoom, 2);
            ImGui::EndMenu();
        }
    }
    ImGui::EndMainMenuBar();
    // Workaround for https://github.com/ocornut/imgui/issues/331
    if (popupNewMap) ImGui::OpenPopup("New Map");
    if (popupNewTileset) ImGui::OpenPopup("New Tileset");
    if (popupPreferences) ImGui::OpenPopup("Preferences");

    if (ImGui::BeginPopup("New Map")) {
        ImGui::Text("Unsaved changes will be lost. Are you sure?");
        if (ImGui::Button("Discard Changes")) {
            selectedEntity = -1;
            pxm_fname = "untitled.pxm";
            pxe_fname = "untitled.pxe";
            tsc_fname = "untitled.tsc";
            pxm.Resize(20, 15);
            pxm.Clear();
            pxe.Resize(1);
            pxe.Clear();
            tsc_text[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGuiFileDialog::Instance()->Display("OpenMapFile")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            OpenMap(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGuiFileDialog::Instance()->Display("SaveMapFile")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            pxm_fname = ImGuiFileDialog::Instance()->GetFilePathName();
            pxe_fname = pxm_fname.substr(0, pxm_fname.find_last_of('.')) + ".pxe";
            SaveMap();
            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGuiFileDialog::Instance()->Display("SaveScriptFile")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            tsc_fname = ImGuiFileDialog::Instance()->GetFilePathName();
            SaveScript();
            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGui::BeginPopup("New Tileset")) {
        ImGui::Text("Unsaved changes will be lost. Are you sure?");
        if (ImGui::Button("Discard Changes")) {
            selectedTile = 0;
            tileRange[0] = tileRange[1] = 0;
            tileRange[2] = tileRange[3] = 1;
            tileset_image = 0;
            tileset_width = 0;
            tileset_height = 0;
            tileset_fname = "untitled.png";
            pxa_fname = "untitled.pxa";
            memset(pxa, 0, PXA_MAX);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGuiFileDialog::Instance()->Display("OpenTilesetFile")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            OpenTileset(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGuiFileDialog::Instance()->Display("OpenNPCList")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            LoadNpcList(ImGuiFileDialog::Instance()->GetFilePathName());
            ImGuiFileDialog::Instance()->Close();
        }
    }

    if (ImGui::BeginPopup("Preferences")) {
        if(ImGui::CollapsingHeader("Background")) {
            ImGui::RadioButton("Checkerboard", &pref.backGraphic, 0);
            ImGui::RadioButton("Solid Color", &pref.backGraphic, 1);
            if (pref.backGraphic) {
                ImGui::ColorPicker4("Color", pref.backColor, ImGuiColorEditFlags_NoAlpha);
            }
        }
        //if(ImGui::CollapsingHeader("File Management")) {
        //    ImGui::Checkbox("Auto load PXE when opening PXM", &pref.autoPXE);
        //    ImGui::Checkbox("Auto load TSC when opening PXM", &pref.autoTSC);
        //    ImGui::Checkbox("Auto load PXA when opening tileset", &pref.autoPXA);
        //}
        if (ImGui::Button("Apply")) {
            pref.Save();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset To Default")) {
            pref.ResetToDefault();
        }
        ImGui::EndPopup();
    }

    ImGui::DockSpaceOverViewport();

    int map_mouse_x, map_mouse_y, map_tile_x, map_tile_y; // Need to remember for status window
    ImGui::Begin("Map", NULL, ImGuiWindowFlags_NoMove);
    {
        // Remake framebuffer if the map size changed
        int ww = pxm.Width() * 16;
        int hh = pxm.Height() * 16;
        if(lastMapW != pxm.Width() || lastMapH != pxm.Height()) {
            CreateMapFB(ww, hh);
            lastMapW = pxm.Width();
            lastMapH = pxm.Height();
        }
        map_mouse_x = int(io.MousePos.x - ImGui::GetWindowPos().x - ImGui::GetCursorPos().x + ImGui::GetScrollX() - 2);
        map_mouse_y = int(io.MousePos.y - ImGui::GetWindowPos().y - ImGui::GetCursorPos().y + ImGui::GetScrollY() - 2);
        map_tile_x = map_mouse_x / (16 * pref.mapZoom);
        map_tile_y = map_mouse_y / (16 * pref.mapZoom);
        // Fix "negative zero"
        if(map_mouse_x < 0) map_tile_x -= 1;
        if(map_mouse_y < 0) map_tile_y -= 1;

        SetMapFB();
        {
            glViewport(0, 0, ww, hh);
            glUniform2f(uf_scale, 2.0f / ww, -2.0f / hh);
            glUniform2f(uf_offset, -1, 1);
            glClearColor(pref.backColor[0], pref.backColor[1], pref.backColor[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            chkerr(__LINE__);
            if(pref.backGraphic == 0) DrawBack(pxm.Width(), pxm.Height());
            if (tileset_image && tileset_width && tileset_height) {
                glBindTexture(GL_TEXTURE_2D, tileset_image);
                for (int y = 0; y < pxm.Height(); y++) {
                    for (int x = 0; x < pxm.Width(); x++) {
                        int tx = pxm.Tile(x, y) % 16;
                        int ty = pxm.Tile(x, y) / 16;
                        DrawRectEx(float(x) * 16, float(y) * 16, 16, 16,
                                   float(tx) / tileset_width, float(ty) / tileset_height,
                                   1.0f / tileset_width, 1.0f / tileset_height, 0xFFFFFFFF);
                    }
                }
            }
            if(pref.editMode == EDIT_ENTITY) {
                glBindTexture(GL_TEXTURE_2D, white_tex);
                for (int i = 0; i < pxe.Size(); i++) {
                    Entity e = pxe.GetEntity(i);
                    DrawRect(float(e.x) * 16, float(e.y) * 16, 16, 16, 0x7700FF00);
                    if(selectedEntity == i) DrawUnfilledRect(float(e.x) * 16, float(e.y) * 16, 16, 16, 0xFF0000FF);
                }
            }
            if(ImGui::IsWindowFocused() && map_tile_x >= 0 && map_tile_x < pxm.Width() && map_tile_y >= 0 && map_tile_y < pxm.Height()) {
                glBindTexture(GL_TEXTURE_2D, white_tex);
                switch(pref.editMode) {
                    case EDIT_PENCIL: // Insert
                        DrawRect(float(map_tile_x) * 16, float(map_tile_y) * 16,
                                 tileRange[2] * 16, tileRange[3] * 16, 0x99FFCC77);
                        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            for (int y = 0; y < tileRange[3]; y++) {
                                for (int x = 0; x < tileRange[2]; x++) {
                                    uint16_t xx = map_tile_x + x;
                                    uint16_t yy = map_tile_y + y;
                                    uint16_t tx = tileRange[0] + x;
                                    uint16_t ty = tileRange[1] + y;
                                    if (xx < pxm.Width() && yy < pxm.Height()) {
                                        pxm.SetTile(xx, yy, ty * tileset_width + tx);
                                    }
                                }
                            }
                        }
                        break;
                    case EDIT_ERASER: // Delete
                        DrawRect(float(map_tile_x) * 16, float(map_tile_y) * 16, 16, 16, 0x997777FF);
                        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            pxm.SetTile(map_tile_x, map_tile_y, 0);
                        }
                        break;
                    case EDIT_ENTITY: // Select Entity
                        DrawUnfilledRect(float(map_tile_x) * 16, float(map_tile_y) * 16, 16, 16, 0xFF00FF00);
                        if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            selectedEntity = pxe.FindEntity(map_tile_x, map_tile_y);
                        }
                        break;
                }
            }
            if(pref.showGrid) DrawGrid(pxm.Width(), pxm.Height());
        }
        SetDefaultFB();

        ImGui::Image((ImTextureID) map_tex, ImVec2(ww * pref.mapZoom, hh * pref.mapZoom), ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::End();

    ImGui::Begin("Entity List", NULL, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove);
    {
        if(ImGui::BeginListBox("##EntityList", ImGui::GetContentRegionAvail())) {
            for(int i = 0; i < pxe.Size(); i++) {
                Entity e = pxe.GetEntity(i);
                char text[120];
                snprintf(text, 120, "%d: (%03hu, %03hu) %04hu, %04hu, %03hu", i, e.x, e.y, e.id, e.event, e.type);
                if(ImGui::Selectable(text, selectedEntity == i)) {
                    selectedEntity = i;
                }
            }
            ImGui::EndListBox();
        }
    }
    ImGui::End();

    int ts_mouse_x, ts_mouse_y, ts_tile_x, ts_tile_y; // Need to remember for status window
    ImGui::Begin("Tileset", NULL, ImGuiWindowFlags_NoMove);
    {
        ts_mouse_x = int(io.MousePos.x - ImGui::GetWindowPos().x - ImGui::GetCursorPos().x + ImGui::GetScrollX() - 2);
        ts_mouse_y = int(io.MousePos.y - ImGui::GetWindowPos().y - ImGui::GetCursorPos().y + ImGui::GetScrollY() - 2);
        ts_tile_x = ts_mouse_x / 32;
        ts_tile_y = ts_mouse_y / 32;
        // Fix "negative zero"
        if(ts_tile_x < 0) ts_tile_x -= 1;
        if(ts_tile_y < 0) ts_tile_y -= 1;

        SetTilesetFB();
        {
            glViewport(0, 0, 256, 128);
            glUniform2f(uf_scale, 2.0f / 256, -2.0f / 128);
            glUniform2f(uf_offset, -1, 1);
            glClearColor(pref.backColor[0], pref.backColor[1], pref.backColor[2], 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            if(pref.backGraphic == 0) DrawBack(tileset_width, tileset_height);
            if (tileset_image) {
                glBindTexture(GL_TEXTURE_2D, tileset_image);
                int x = 0, y = 0;
                int tx = 0, ty = 0;
                for (int i = 0; i < tileset_width * tileset_height; i++) {
                    DrawRectEx(float(x) * 16, float(y) * 16, 16, 16,
                               float(tx) / tileset_width, float(ty) / tileset_height,
                               1.0f / tileset_width, 1.0f / tileset_height, 0xFFFFFFFF);
                    // Width of texture and tileset window may differ, so need to iterate separately
                    if (++x == 16) {
                        x = 0;
                        y++;
                    }
                    if (++tx == tileset_width) {
                        tx = 0;
                        ty++;
                    }
                }
                if(ts_tile_x >= 0 && ts_tile_x < 16 && ts_tile_y >= 0 && ts_tile_y < tileset_height) {
                    glBindTexture(GL_TEXTURE_2D, white_tex);
                    DrawRect(float(ts_tile_x) * 16, float(ts_tile_y) * 16, 16, 16, 0x99FFFFFF);
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        tileRange[0] = ts_tile_x;
                        tileRange[1] = ts_tile_y;
                        tileRange[2] = 1;
                        tileRange[3] = 1;
                        selectedTile = ts_tile_y * 16 + ts_tile_x;
                    }
                    if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if(ts_tile_x >= tileRange[0] && ts_tile_y >= tileRange[1]) {
                            tileRange[2] = 1 + ts_tile_x - tileRange[0];
                            tileRange[3] = 1 + ts_tile_y - tileRange[1];
                        } else {
                            tileRange[0] = ts_tile_x;
                            tileRange[1] = ts_tile_y;
                            tileRange[2] = 1;
                            tileRange[3] = 1;
                            selectedTile = ts_tile_y * 16 + ts_tile_x;
                        }
                    }
                }
            }
            glBindTexture(GL_TEXTURE_2D, white_tex);
            DrawUnfilledRect(float(tileRange[0]) * 16, float(tileRange[1]) * 16, 16, 16, 0xFF0000FF);
            DrawUnfilledRect(float(tileRange[0]) * 16, float(tileRange[1]) * 16,
                             float(tileRange[2]) * 16, float(tileRange[3]) * 16, 0xFF00FF00);
            glBindTexture(GL_TEXTURE_2D, tileset_image);
            chkerr(__LINE__);
            if(pref.showGrid) DrawGrid(16, 8);
        }
        SetDefaultFB();
        ImGui::Image((ImTextureID) tileset_tex, ImVec2(512, 256), ImVec2(0, 1), ImVec2(1, 0));
        // Tile attributes
        if(ImGui::CollapsingHeader("Tile Attributes")) {
            uint8_t attr = pxa[selectedTile];
            bool attr_flag[4];
            for(int i = 0; i < 4; i++) {
                attr_flag[i] = (attr >> (4 + i)) & 1;
            }
            if(ImGui::BeginTable("AttrFlags", 4)) {
                ImGui::TableNextColumn(); ImGui::Checkbox("Slope", &attr_flag[0]);
                ImGui::TableNextColumn(); ImGui::Checkbox("Water", &attr_flag[1]);
                ImGui::TableNextColumn(); ImGui::Checkbox("Fore", &attr_flag[2]);
                ImGui::TableNextColumn(); ImGui::Checkbox("Wind", &attr_flag[3]);
                ImGui::EndTable();
            }
            if(attr_flag[3]) {
                int dir = attr & 3;
                if(ImGui::BeginTable("AttrWind", 4)) {
                    ImGui::TableNextColumn(); ImGui::RadioButton("Left", &dir, 0);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Up", &dir, 1);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Right", &dir, 2);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Down", &dir, 3);
                    ImGui::EndTable();
                }
            } else if(attr_flag[0]) {
                int dir = attr & 7;
                if(ImGui::BeginTable("AttrSlope", 2)) {
                    ImGui::TableNextColumn(); ImGui::RadioButton("Ceiling Left High", &dir, 0);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Ceiling Left Low", &dir, 1);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Ceiling Right Low", &dir, 2);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Ceiling Right High", &dir, 3);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Floor Left High", &dir, 4);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Floor Left Low", &dir, 5);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Floor Right Low", &dir, 6);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Floor Right High", &dir, 7);
                    ImGui::EndTable();
                }
            } else {
                int val = attr & 7;
                if(ImGui::BeginTable("AttrVal", 2)) {
                    ImGui::TableNextColumn(); ImGui::RadioButton("N/A", &val, 0);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Solid", &val, 1);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Damage", &val, 2);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Breakable", &val, 3);
                    ImGui::TableNextColumn(); ImGui::RadioButton("NPC Solid", &val, 4);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Bullet Pass", &val, 5);
                    ImGui::TableNextColumn(); ImGui::RadioButton("Player Solid", &val, 6);
                    ImGui::TableNextColumn(); ImGui::RadioButton("N/A", &val, 7);
                    ImGui::EndTable();
                }
            }
        }
    }
    ImGui::End();

    ImGui::Begin("Entity", NULL, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove);
    {
        if(selectedEntity >= 0) {
            Entity e = pxe.GetEntity(selectedEntity);
            int x = e.x, y = e.y, id = e.id, ev = e.event, npc = e.type;
            ImGui::InputInt("X", &x, 1, 10);
            ImGui::InputInt("Y", &y, 1, 10);
            ImGui::InputInt("ID", &id, 1, 10);
            ImGui::InputInt("EV", &ev, 1, 10);
            ImGui::InputInt("NPC", &npc, 1, 10);
            e.x = std::clamp(x, 0, pxm.Width() - 1);
            e.y = std::clamp(y, 0, pxm.Height() - 1);
            e.id = std::clamp(id, 0, 9999);
            e.event = std::clamp(ev, 0, 9999);
            e.type = std::clamp(npc, 0, 361);
            if(ImGui::CollapsingHeader("Entity Flags")) {
                bool flags[16];
                for(int i = 0; i < 16; i++) {
                    flags[i] = (e.flags >> i) & 1;
                }
                if(ImGui::BeginTable("Flags", 2)) {
                    ImGui::TableNextColumn(); ImGui::Checkbox("Solid (Mushy)",          &flags[0]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Ignore NPC Solid Tiles", &flags[1]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Invulnerable",           &flags[2]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Ignore Solid Tiles",     &flags[3]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Bouncy Top",             &flags[4]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Shootable",              &flags[5]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Solid (Brick)",          &flags[6]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Only Front Dmg Player",  &flags[7]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Option 1",               &flags[8]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Call Event On Death",    &flags[9]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Drop Power Ups",         &flags[10]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Enable On Flag",         &flags[11]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Option 2 (Face Right)",  &flags[12]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Interactive",            &flags[13]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Disable On Flag",        &flags[14]);
                    ImGui::TableNextColumn(); ImGui::Checkbox("Show Damage",            &flags[15]);
                    ImGui::EndTable();
                }
                e.flags = 0;
                for(int i = 0; i < 16; i++) {
                    if(flags[i]) e.flags |=  1 << i;
                }
            }
            pxe.SetEntity(selectedEntity, e);
            if(ImGui::CollapsingHeader("Sprite Preview")) {
                if(!pref.npcListPath.empty() && npc_sprites.size() > e.type) {
                    NpcSprite s = npc_sprites[e.type];
                    if (s.texture) {
                        ImGui::Image((ImTextureID) s.texture, ImVec2(s.tex_w, s.tex_h));
                    } else {
                        ImGui::Text("No sprite.");
                    }
                } else {
                    ImGui::Text("NPC sprites not loaded.");
                }
            }
            if(ImGui::CollapsingHeader("Script Preview")) {
                char look_for[8];
                snprintf(look_for, 8, "#%04hu", pxe.GetEntity(selectedEntity).event);
                std::string tsc = tsc_text;
                size_t pos = tsc.find(look_for);
                if(pos != std::string::npos && (pos == 0 || (pos > 0 && tsc[pos-1] == '\n'))) {
                    char tsc_cut[1024];
                    int i = 0;
                    for(; i < 1023; i++) {
                        if(pos >= tsc.length()) break;
                        if(tsc[pos] == '#' && i > 0 && tsc[pos-1] == '\n') break;
                        if(tsc[pos] == '\n' && i > 0 && tsc[pos-1] == '\n') break;
                        tsc_cut[i] = tsc[pos++];
                    }
                    tsc_cut[i] = 0;
                    ImGui::Text("%s", tsc_cut);
                }
            }
        } else {
            ImGui::Text("No entity selected.");
        }
    }
    ImGui::End();

    ImGui::Begin("Script", NULL, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove);
    {
        if(tsc_text[0]) {
            ImGui::InputTextMultiline("##ScriptEdit", tsc_text, TSC_MAX, ImGui::GetContentRegionAvail());
        }
    }
    ImGui::End();

    ImGuiWindowClass winclass;
    winclass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&winclass);
    ImGui::Begin("Status", NULL, ImGuiWindowFlags_NoMove);
    {
        // PXM
        size_t subpos = pxm_fname.find_last_of('/');
        if(subpos) subpos++;
        ImGui::Text("%s", pxm_fname.substr(subpos).c_str());
        ImGui::SameLine();
        if(map_tile_x >= 0 && map_tile_x < pxm.Width() && map_tile_y >= 0 && map_tile_y < pxm.Height()) {
            ImGui::Text("[%03d, %03d]", map_tile_x, map_tile_y);
        } else {
            ImGui::Text("[---, ---]");
        }
        ImGui::SameLine();
        ImGui::Text("/ (%03hu, %03hu) -", pxm.Width(), pxm.Height());
        ImGui::SameLine();
        // PXE
        subpos = pxe_fname.find_last_of('/');
        if(subpos) subpos++;
        ImGui::Text("%s", pxe_fname.substr(subpos).c_str());
        ImGui::SameLine();
        if(/*editMode == EDIT_ENTITY && */selectedEntity >= 0) {
            ImGui::Text("[%03d] / %03hu -", selectedEntity, pxe.Size());
        } else {
            ImGui::Text("[---] / %03hu -", pxe.Size());
        }
        ImGui::SameLine();
        // TSC
        subpos = tsc_fname.find_last_of('/');
        if(subpos) subpos++;
        ImGui::Text("%s", tsc_fname.substr(subpos).c_str());
        ImGui::SameLine();
        if(tsc_obfuscated) {
            ImGui::Text("(obfuscated)");
        } else {
            ImGui::Text("(plaintext)");
        }
        ImGui::SameLine();
        ImGui::Text("-");
        ImGui::SameLine();
        // Tileset
        subpos = tileset_fname.find_last_of('/');
        if(subpos) subpos++;
        ImGui::Text("%s", tileset_fname.substr(subpos).c_str());
        ImGui::SameLine();
        ImGui::Text("-");
        ImGui::SameLine();
        // PXA
        subpos = pxa_fname.find_last_of('/');
        if(subpos) subpos++;
        ImGui::Text("%s", pxa_fname.substr(subpos).c_str());
    }
    ImGui::End();

    return menuExit;
}
