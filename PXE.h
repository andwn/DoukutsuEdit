#ifndef STAGE9_PXE_H
#define STAGE9_PXE_H

typedef struct {
    uint16_t x, y, id, event, type, flags;
} Entity;

class PXE {
public:
    PXE() {
        entities = NULL;
        Clear();
    }
    ~PXE() { free(entities); }
    uint16_t Size() const { return size; }
    Entity GetEntity(uint16_t i) { return i < size ? entities[i] : Entity(); }
    int FindEntity(uint16_t x, uint16_t y);

    void SetEntity(uint16_t i, Entity e);
    void Resize(uint16_t _size);
    void AddEntity(Entity e);
    void Clear();
    void Load(FILE *file);
    void Save(FILE *file);
private:
    uint16_t size;
    Entity *entities;
};

#endif //STAGE9_PXE_H
