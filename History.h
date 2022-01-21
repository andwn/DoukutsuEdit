#ifndef STAGE9_HISTORY_H
#define STAGE9_HISTORY_H

enum {
    MAP_MOD, MAP_SIZE, ENTITY_MOD, ENTITY_ADD, ENTITY_DEL
};

typedef struct {
    int action;
    union {
        struct { uint16_t x, y, w, h, tx, ty, *old_data; } map_mod;
        struct { uint16_t old_w, old_h, new_w, new_h, *old_data; } map_size;
        struct { Entity old_entity, new_entity; } entity_mod;
        struct { Entity new_entity; } entity_add;
        struct { Entity old_entity; uint16_t index; } entity_del;
    };
} HistEntry;

class History {
public:
    History();
    ~History();

    void AddEntry(HistEntry *entry);

    void Clear();
    void ClearUndoList();
    void ClearRedoList();

    HistEntry* Undo();
    HistEntry* Redo();

private:
    std::list<HistEntry*> undoList;
    std::list<HistEntry*> redoList;
};

#endif //STAGE9_HISTORY_H
