#include "common.h"
#include "PXE.h"

int PXE::FindEntity(uint16_t x, uint16_t y) {
    for(int i = 0; i < size; i++) {
        Entity e = entities[i];
        if(e.x == x && e.y == y) return i;
    }
    return -1;
}

void PXE::SetEntity(uint16_t i, Entity e) {
    if(i < size) memcpy(&entities[i], &e, sizeof(Entity));
}

void PXE::Resize(uint16_t _size) {
    Entity *temp = (Entity*) calloc(_size, sizeof(Entity));
    if(entities) {
        memcpy(temp, entities, min(size, _size) * sizeof(Entity));
        free(entities);
    }
    entities = temp;
    size = _size;
}

void PXE::AddEntity(Entity e) {
    Resize(size + 1);
    memcpy(&entities[size - 1], &e, sizeof(Entity));
}

void PXE::DeleteEntity(uint16_t index) {
    for(uint16_t i = index; i < size - 1; i++) entities[i] = entities[i+1];
    Resize(size - 1);
}

void PXE::Clear() {
    Resize(1);
    SetEntity(0, Entity());
}

void PXE::Load(FILE *file) {
    char head[4];
    fread(head, 1, 4, file);
    fread(&size, 2, 1, file);
    fseek(file, 2, SEEK_CUR); // skip 2 bytes
    free(entities);
    entities = (Entity*) calloc(size, sizeof(Entity));
    fread(entities, 2, size * 6, file);
    //printf("Size: %hu\n", size);
    //for(uint16_t i = 0; i < size; i++) {
    //    Entity e = entities[i];
    //    printf("Entity: %hu, %hu, %hu, %hu, %hu, %hu\n", e.x, e.y, e.id, e.event, e.type, e.flags);
    //}
}

void PXE::Save(FILE *file) {
    static const char head[4] = "PXE";
    fwrite(head, 1, 4, file);
    fwrite(&size, 2, 1, file);
    fwrite(&size, 2, 1, file); // skip 2 bytes
    fwrite(entities, 2, size * 6, file);
}
