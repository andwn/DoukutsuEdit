#include "common.h"
#include <SDL.h>
#include "ini.h"

#include "Preferences.h"

static int PrefIniHandler(void* user, const char* section, const char* name, const char* value) {
#define SECTION(s) if(strcmp(section, s) == 0)
#define PREF(n, action) if(strcmp(name, n) == 0) { action; return 1; }
    Preferences *p = (Preferences*) user;
    SECTION("Background") {
        PREF("graphic", p->backGraphic = atoi(value));
        PREF("colorR", p->backColor[0] = atof(value));
        PREF("colorG", p->backColor[1] = atof(value));
        PREF("colorB", p->backColor[2] = atof(value));
    }
    SECTION("Behavior") {
        //PREF("autoPXE", p->autoPXE = atoi(value));
        //PREF("autoTSC", p->autoTSC = atoi(value));
        //PREF("autoPXA", p->autoPXA = atoi(value));
        PREF("npcListPath", p->npcListPath = value);
    }
    SECTION("RecentPXM") {
        char str[8] = "FILE0";
        for(int i = 0; i < 10; i++) {
            str[4] = '0' + i;
            PREF(str, p->recentPXM[i] = value);
        }
    }
    SECTION("RecentTS") {
        char str[8] = "FILE0";
        for(int i = 0; i < 10; i++) {
            str[4] = '0' + i;
            PREF(str, p->recentTS[i] = value);
        }
    }
    return 0;
}

Preferences::Preferences() {
    char *prefPath = SDL_GetPrefPath("Skychase", "DoukutsuEdit");
    savePath = std::string(prefPath) + "preferences.ini";
    SDL_free(prefPath);
    Load();
}

void Preferences::ResetToDefault() {
    backGraphic = 0;
    backColor[0] = backColor[1] = backColor[2] = 0;
    //autoPXE = true;
    //autoTSC = true;
    //autoPXA = true;
    npcListPath = "";
    for(int i = 0; i < 10; i++) {
        recentPXM[i] = "";
        recentTS[i] = "";
    }
    editMode = EDIT_PENCIL;
    mapZoom = 2;
    showGrid = true;
}

void Preferences::Load() {
    ResetToDefault();
    FILE *file = fopen(savePath.c_str(), "r");
    if(file) {
        ini_parse_file(file, PrefIniHandler, this);
        fclose(file);
    }
}

void Preferences::Save() {
    FILE *file = fopen(savePath.c_str(), "w");
    if(file) {
        fprintf(file, "[Background]\n");
        fprintf(file, "graphic = %d\n", backGraphic);
        fprintf(file, "colorR = %f\n", backColor[0]);
        fprintf(file, "colorG = %f\n", backColor[1]);
        fprintf(file, "colorB = %f\n", backColor[2]);
        fprintf(file, "\n");
        fprintf(file, "[Behavior]\n");
        //fprintf(file, "autoPXE = %d\n", autoPXE);
        //fprintf(file, "autoTSC = %d\n", autoTSC);
        //fprintf(file, "autoPXA = %d\n", autoPXA);
        fprintf(file, "npcListPath = %s\n", npcListPath.c_str());
        fprintf(file, "\n");
        fprintf(file, "[RecentPXM]\n");
        for(int i = 0; i < 10; i++) {
            fprintf(file, "FILE%d = %s\n", i, recentPXM[i].c_str());
        }
        fprintf(file, "\n");
        fprintf(file, "[RecentTS]\n");
        for(int i = 0; i < 10; i++) {
            fprintf(file, "FILE%d = %s\n", i, recentTS[i].c_str());
        }
        fprintf(file, "\n");
        fprintf(file, "[EditorState]\n");
        fprintf(file, "editMode = %d\n", editMode);
        fprintf(file, "mapZoom = %d\n", mapZoom);
        fprintf(file, "showGrid = %d\n", showGrid);

        fclose(file);
    }
}

void Preferences::AddRecentPXM(std::string fn) {
    // Remove filename from the list if it's already in there
    for(int i = 0; i < 10; i++) {
        if(recentPXM[i] == fn) {
            for(int j = i; j < 9; j++) recentPXM[j] = recentPXM[j+1];
            break;
        }
    }
    // Push list back and add new filename to front
    for(int i = 9; i > 0; i--) recentPXM[i] = recentPXM[i-1];
    recentPXM[0] = fn;
    Save();
}

void Preferences::AddRecentTS(std::string fn) {
    for(int i = 0; i < 10; i++) {
        if(recentTS[i] == fn) {
            for(int j = i; j < 9; j++) recentTS[j] = recentTS[j+1];
            break;
        }
    }
    for(int i = 9; i > 0; i--) recentTS[i] = recentTS[i-1];
    recentTS[0] = fn;
    Save();
}
