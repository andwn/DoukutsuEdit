#include "common.h"
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
        "#version 150\n"
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
        "#version 150\n"
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
    map_zoom = 2;
    lastMapW = lastMapH = 0;
    map_fb = tileset_fb = 0;
    clickingMapX = clickingMapY = 0;
    tileset_image = 0;
    tileset_width = tileset_height = 0;
    selectedEntity = -1;
    clickingTile = selectedTile = 0;
    editMode = EDIT_PENCIL;
    tsc_text[0] = 0;
    tsc_obfuscated = false;
    CreateTilesetFB();
    glGenTextures(1, &tileset_image);
    // Blank white texture
    glGenTextures(1, &white_tex);
    glBindTexture(GL_TEXTURE_2D, white_tex);
    static const uint32_t white[4] = { 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    chkerr(__LINE__);
    InitShaders();

    Preferences &pref = Preferences::Instance();
    if(pref.recentPXM[0].length() > 0) OpenMap(pref.recentPXM[0]);
    if(pref.recentTS[0].length() > 0) OpenTileset(pref.recentTS[0]);
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
                tsc_text[i] += key;
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

void StageWindow::OpenTileset(std::string fname) {
    FILE *file = fopen(fname.c_str(), "rb");
    if(file) {
        tileset_fname = fname;
        int x, y, c;
        stbi_uc *rgba_data = stbi_load_from_file(file, &x, &y, &c, STBI_rgb_alpha);
        if(rgba_data) {
            tileset_width = x / 16;
            tileset_height = y / 16;
            glBindTexture(GL_TEXTURE_2D, tileset_image);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_data);
            chkerr(__LINE__);
            stbi_image_free(rgba_data);
        }
        fclose(file);
    }
    // Try to open PXA with the same base name
    selectedTile = 0;
    std::string newfn = fname.substr(0, fname.find_last_of('.')) + ".pxa";
    size_t prt_loc = newfn.find("Prt");
    if(prt_loc != std::string::npos) newfn = newfn.erase(prt_loc, 3);
    //printf("%s\n", newfn.c_str());
    file = fopen(newfn.c_str(), "rb");
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
            ImGui::RadioButton("Insert Mode", &editMode, EDIT_PENCIL);
            ImGui::RadioButton("Erase Mode", &editMode, EDIT_ERASER);
            ImGui::RadioButton("Entity Mode", &editMode, EDIT_ENTITY);
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
            pxa_fname = pxm_fname.substr(0, pxm_fname.find_last_of('.')) + ".pxe";
            SaveMap();
            ImGuiFileDialog::Instance()->Close();
        }
    }
    if (ImGui::BeginPopup("New Tileset")) {
        ImGui::Text("Unsaved changes will be lost. Are you sure?");
        if (ImGui::Button("Discard Changes")) {
            selectedTile = 0;
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

    if (ImGui::BeginPopup("Preferences")) {
        ImGui::Checkbox("Automatically load PXE when opening PXM", &pref.autoPXE);
        ImGui::Checkbox("Automatically load TSC when opening PXM", &pref.autoTSC);
        ImGui::Checkbox("Automatically load PXA when opening a tileset image", &pref.autoPXA);
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
    ImGui::Begin("Map");
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
        map_tile_x = map_mouse_x / (16 * map_zoom);
        map_tile_y = map_mouse_y / (16 * map_zoom);
        // Fix "negative zero"
        if(map_mouse_x < 0) map_tile_x -= 1;
        if(map_mouse_y < 0) map_tile_y -= 1;

        SetMapFB();
        {
            glViewport(0, 0, ww, hh);
            glUniform2f(uf_scale, 2.0f / ww, -2.0f / hh);
            glUniform2f(uf_offset, -1, 1);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            chkerr(__LINE__);
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
            if(editMode == EDIT_ENTITY) {
                glBindTexture(GL_TEXTURE_2D, white_tex);
                for (int i = 0; i < pxe.Size(); i++) {
                    Entity e = pxe.GetEntity(i);
                    DrawRect(float(e.x) * 16, float(e.y) * 16, 16, 16, 0x7700FF00);
                }
            }
            if(ImGui::IsWindowFocused() && map_tile_x >= 0 && map_tile_x < pxm.Width() && map_tile_y >= 0 && map_tile_y < pxm.Height()) {
                glBindTexture(GL_TEXTURE_2D, white_tex);
                uint32_t col = editMode == EDIT_ERASER ? 0x77AAAAFF : 0x77FFCC77;
                DrawRect(float(map_tile_x) * 16, float(map_tile_y) * 16, 16, 16, col);
                if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    clickingMapX = map_tile_x;
                    clickingMapY = map_tile_y;
                }
                if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && clickingMapX == map_tile_x && clickingMapY == map_tile_y) {
                    switch(editMode) {
                        case EDIT_PENCIL: // Insert
                            pxm.SetTile(map_tile_x, map_tile_y, selectedTile);
                            break;
                        case EDIT_ERASER: // Delete
                            pxm.SetTile(map_tile_x, map_tile_y, 0);
                            break;
                        case EDIT_ENTITY: // Select Entity
                            selectedEntity = pxe.FindEntity(map_tile_x, map_tile_y);
                            break;
                    }
                }
            }
            DrawGrid(pxm.Width(), pxm.Height());
        }
        SetDefaultFB();

        ImGui::Image((ImTextureID) map_tex, ImVec2(ww * map_zoom, hh * map_zoom), ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::End();

    ImGui::Begin("Entity List", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
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
    ImGui::Begin("Tileset");
    {
        ts_mouse_x = int(io.MousePos.x - ImGui::GetWindowPos().x - ImGui::GetCursorPos().x + ImGui::GetScrollX() - 2);
        ts_mouse_y = int(io.MousePos.y - ImGui::GetWindowPos().y - ImGui::GetCursorPos().y + ImGui::GetScrollY() - 2);
        ts_tile_x = ts_mouse_x / (16 * map_zoom);
        ts_tile_y = ts_mouse_y / (16 * map_zoom);
        // Fix "negative zero"
        if(ts_tile_x < 0) ts_tile_x -= 1;
        if(ts_tile_y < 0) ts_tile_y -= 1;

        SetTilesetFB();
        {
            glViewport(0, 0, 256, 128);
            glUniform2f(uf_scale, 2.0f / 256, -2.0f / 128);
            glUniform2f(uf_offset, -1, 1);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            if (tileset_image) {
                glBindTexture(GL_TEXTURE_2D, tileset_image);
                int x = 0, y = 0;
                int tx = 0, ty = 0;
                for (int i = 0; i < tileset_width * tileset_height; i++) {
                    DrawRectEx(float(x) * 16, float(y) * 16, 16, 16,
                               float(tx) / tileset_width, float(ty) / tileset_height,
                               1.0f / tileset_width, 1.0f / tileset_height, 0xFFFFFFFF);
                    if(selectedTile == i) {
                        glBindTexture(GL_TEXTURE_2D, white_tex);
                        DrawRect(float(x) * 16, float(y) * 16, 16, 16, 0x77FFFFFF);
                        glBindTexture(GL_TEXTURE_2D, tileset_image);
                    }
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
                    DrawRect(float(map_tile_x) * 16, float(map_tile_y) * 16, 16, 16, 0x77FFFFFF);
                    uint16_t thisTile = ts_tile_y * 16 + ts_tile_x;
                    if(ImGui::IsMouseDown(ImGuiMouseButton_Left)) clickingTile = thisTile;
                    if(ImGui::IsMouseReleased(ImGuiMouseButton_Left) && clickingTile == thisTile)
                        selectedTile = thisTile;
                }
            }
            chkerr(__LINE__);
            DrawGrid(16, 8);
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

    ImGui::Begin("Entity", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
    {
        if(/*editMode == EDIT_ENTITY && */selectedEntity >= 0) {
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

    ImGui::Begin("Script", NULL, ImGuiWindowFlags_NoFocusOnAppearing);
    {
        if(tsc_text[0]) {
            ImGui::InputTextMultiline("##ScriptEdit", tsc_text, TSC_MAX, ImGui::GetContentRegionAvail());
        }
    }
    ImGui::End();

    ImGuiWindowClass winclass;
    winclass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
    ImGui::SetNextWindowClass(&winclass);
    ImGui::Begin("Status");
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
