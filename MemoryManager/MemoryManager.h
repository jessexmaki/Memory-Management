#include <climits>
#include <cmath>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using namespace std;

int bestFit(int sizeInWords, void *list);
int worstFit(int sizeInWords, void *list);

struct LL_Node {
    LL_Node* prev;
    LL_Node* next;
    int head;
    bool hole;
    int size;
    
LL_Node(LL_Node* _prev, LL_Node* _next, int _head, bool _hole, int _size) {
    this->prev = _prev;
    this->next = _next;
    this->head = _head;
    this->hole = _hole;
    this->size = _size;
    }
};

class MemoryManager {

public:
    std::function<int(int,void*)> allocator;
    LL_Node* node = nullptr;
    uint8_t* start = nullptr;
    uint8_t* bit_map = nullptr;
    uint16_t* list = nullptr;
    unsigned word_size;

    size_t size_in_words;
    unsigned mem = word_size * size_in_words;
    
    void shutdown();
    MemoryManager(unsigned wordSize, std::function<int(int,void*)> allocator);
    ~MemoryManager();
    void initialize(size_t sizeInWords);
    void *allocate(size_t sizeInBytes);
    void free(void *address);
    void setAllocator(std::function<int(int,void *)> allocator);
    int dumpMemoryMap(char *filename);
    void *getList();
    void *getBitmap();
    unsigned getWordSize();
    void *getMemoryStart();
    unsigned getMemoryLimit();
    void updateList();
    void updateBitmap();
    void combine_holes();
    
};
