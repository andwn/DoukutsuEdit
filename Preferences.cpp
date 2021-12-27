#include "common.h"
#include <SDL2/SDL.h>
#include "ini.h"

#include "Preferences.h"

static int PrefIniHandler(void* user, const char* section, const char* name, const char* value) {
#define SECTION(s) if(strcmp(section, s) == 0)
#define PREF(n, action) if(strcmp(name, n) == 0) { action; return 1; }
    Preferences *p = (Preferences*) user;
    SECTION("Behavior") {
        PREF("autoPXE", p->autoPXE = atoi(value));
        PREF("autoTSC", p->autoTSC = atoi(value));
        PREF("autoPXA", p->autoPXA = atoi(value));
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
    autoPXE = true;
    autoTSC = true;
    autoPXA = true;
    for(int i = 0; i < 10; i++) {
        recentPXM[i] = "";
        recentTS[i] = "";
    }
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
        fprintf(file, "[Behavior]\n");
        fprintf(file, "autoPXE = %d\n", autoPXE);
        fprintf(file, "autoTSC = %d\n", autoTSC);
        fprintf(file, "autoPXA = %d\n", autoPXA);
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
        fclose(file);
    }
}

void Preferences::AddRecentPXM(std::string fn) {
    for(int i = 9; i > 0; i--) recentPXM[i] = recentPXM[i-1];
    recentPXM[0] = fn;
}

void Preferences::AddRecentTS(std::string fn) {
    for(int i = 9; i > 0; i--) recentTS[i] = recentTS[i-1];
    recentTS[0] = fn;
}
