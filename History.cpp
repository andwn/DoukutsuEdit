#include "common.h"
#include "pxm.h"
#include "PXE.h"
#include "History.h"

History::History() {

}
History::~History() {
    Clear();
}

void History::AddEntry(HistEntry *entry) {
    undoList.push_front(entry);
    ClearRedoList();
}

void History::ClearUndoList() {
    while(!undoList.empty()) {
        HistEntry *e = undoList.front();
        undoList.pop_front();
        if(e->action == MAP_MOD) free(e->map_mod.old_data);
        if(e->action == MAP_SIZE) free(e->map_size.old_data);
        free(e);
    }
}

void History::ClearRedoList() {
    while(!redoList.empty()) {
        HistEntry *e = redoList.front();
        redoList.pop_front();
        if(e->action == MAP_MOD) free(e->map_mod.old_data);
        if(e->action == MAP_SIZE) free(e->map_size.old_data);
        free(e);
    }
}

void History::Clear() {
    ClearUndoList();
    ClearRedoList();
}

HistEntry* History::Undo() {
    if(undoList.empty()) return NULL;
    HistEntry *e = undoList.front();
    undoList.pop_front();
    redoList.push_front(e);
    return e;
}

HistEntry* History::Redo() {
    if(redoList.empty()) return NULL;
    HistEntry *e = redoList.front();
    redoList.pop_front();
    undoList.push_front(e);
    return e;
}
