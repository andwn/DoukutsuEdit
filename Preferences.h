#ifndef STAGE9_PREFERENCES_H
#define STAGE9_PREFERENCES_H

class Preferences {
public:
    static Preferences& Instance() {
        static Preferences instance;
        return instance;
    }
    Preferences(Preferences const&) = delete;
    void operator=(Preferences const&)  = delete;

    void ResetToDefault();
    void Load();
    void Save();

    void AddRecentPXM(std::string fn);
    void AddRecentTS(std::string fn);

    bool autoPXE, autoTSC, autoPXA;
    std::string recentPXM[10], recentTS[10];

private:
    Preferences();

    std::string savePath;
};

#endif //STAGE9_PREFERENCES_H
