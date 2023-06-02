#include "MemoryManager.h"
using namespace std;

// Constructor; sets native word size (in bytes, for alignment) and default allocator for finding a memory hole.
MemoryManager::MemoryManager(unsigned wordSize, function<int(int,void *)> allocator) {
    this->allocator = allocator;
    this->word_size = wordSize;
}

// Releases all memory allocated by this object without leaking memory
MemoryManager::~MemoryManager() {
    shutdown();
}

// Instantiates block of requested size, no larger than 65536 words; cleans up previous block if applicable
void MemoryManager::initialize(size_t sizeInwords) {
    if (sizeInwords < 65536 ) {
        this->size_in_words = sizeInwords;
        this->mem = word_size * size_in_words;
        
        if (start) {
            shutdown();
            
        }
        start = (uint8_t *) sbrk(size_in_words * word_size);
    }
}

/* Releases memory block acquired during initialization, if any. This should only include memory created for
 long term use not those that returned such as getList() or getBitmap() as whatever is calling those should
 delete it instead.*/
void MemoryManager::shutdown() {
    sbrk(-(size_in_words * word_size));
    start = nullptr;
    LL_Node* tmp = this->node;
    LL_Node* n;
    
    while(tmp) {
        n = tmp->next;
        delete tmp;
        tmp = n;

    }
    
    this->node = nullptr;
}

// Allocates a memory using the allocator function. If no memory is available or size is invalid, returns nullptr.
void* MemoryManager::allocate(size_t sizeInBytes) {
    
    if(!this->node) {
        this->node = new LL_Node(nullptr, nullptr, 0, true, size_in_words);
    }
    
    int l = ceil( (float) sizeInBytes/word_size);
    int offset = allocator(l, (void*)getList());

    if(offset == -1) {
        return nullptr;
    }
    
    LL_Node* temp = this->node;
    while (temp && temp->head != offset) temp = temp->next;
    
    if (temp->size == l) {
        temp->hole = false;
    }
    else {
        LL_Node* _node = temp->next;
        temp->hole = false;
        temp->next = new LL_Node(temp, _node,temp->head + l,true,temp->size - l);
        temp->size = l;
        
    }
    return this->start + (offset * word_size);
}

// Frees the memory block within the memory manager so that it can be reused
void MemoryManager::free(void *address) {
    int offset = ceil(((uint8_t*) address - this->start) / word_size);
    LL_Node* temp = node;
    
    while(temp) {
        if(temp->head == offset){
            temp->hole = true;
        }
        
        temp = temp->next;
    }
    combine_holes();
}

// Changes the allocation algorithm to identifying the memory hole to use for allocation
void MemoryManager::setAllocator(std::function <int(int,void *)> allocator) {
    this->allocator = allocator;
}

// Uses standard POSIX calls to write hole list to filename as text, returning -1 on error and 0 if successful.
int MemoryManager::dumpMemoryMap(char *filename) {
    int file = open(filename, O_CREAT|O_WRONLY, 0600);
    auto* holeList = static_cast<uint16_t*>(getList());
    uint16_t length = *holeList++;
    
    if(file < 0) {
        return -1;
    }
    
    string str;
    
    for(uint16_t i = 0; i < length * 2; i += 2) {
        if(i==0) {
            str += "[" + to_string(holeList[i]) + ", " + to_string(holeList[i+1]) + "]";
        }
        else {
            str += " - [" + to_string(holeList[i]) + ", " + to_string(holeList[i+1]) + "]";
            
        }
        
    }
    
    char* buffer = const_cast<char*>(str.c_str());
    write(file, buffer, str.length());
    close(file);
    delete[] this->list;
    
    return 0;
}

/* Returns an array of information (in decimal) about holes for use by the allocator function (little-Endian).
 Offset and length are in words. If no memory has been allocated, the function should return a NULL pointer.*/
void* MemoryManager::getList() {
    updateList();
    return this->list;
}

/* Returns a bit-stream of bits in terms of an array representing whether words are used (1) or free (0). The
 first two bytes are the size of the bitmap (little-Endian); the rest is the bitmap, word-wise.
 Note : In the following example B0, B2, and B4 are holes, B1 and B3 are allocated memory.*/
void* MemoryManager::getBitmap() {
    updateBitmap();
    return this->bit_map;
}

// Returns the word size used for alignment.
unsigned MemoryManager::getWordSize() {
    return word_size;
}

// Returns the byte-wise memory address of the beginning of the memory block
void* MemoryManager::getMemoryStart() {
    return start;
}

// Returns the byte limit of the current memory block.
unsigned MemoryManager::getMemoryLimit() {
    return mem;
}

void MemoryManager::updateList() {
    vector<uint16_t> temp_vec = {};
    uint16_t l = 0;
    LL_Node *temp = this->node;
    uint16_t counter;
    
    while (temp) {
        if (temp->hole) {
            l++;
            temp_vec.push_back(temp->head);
            temp_vec.push_back(temp->size);
            
        }
        temp = temp->next;
    }
    
    auto list_size = (2 * l) + 1;
    this->list = new uint16_t[list_size];
    this->list[0] = l;
    
    for (counter = 0; counter < l * 2; counter++) {
        this->list[counter + 1] = temp_vec[counter];
    }
}

void MemoryManager::updateBitmap() {
    int l = ceil( (float)size_in_words / 8);
    auto bit_map_size = 2 + l;
    this->bit_map = new uint8_t[bit_map_size];
    LL_Node* temp = this->node;
    vector<bool> temp_vec = {};
    
    while (temp) {
        for (int i = 0; i < temp->size; i++) {
            temp_vec.push_back(!temp->hole);
        }
        temp = temp->next;
    }

    int pos = 2;
    uint8_t cnt = 0;
    
    for (long unsigned int i = 0; i < temp_vec.size(); i += 8) {
        for (long unsigned int j = 0; j < 8; j++) {
            if (i+j < temp_vec.size()) {
                cnt += pow(2, j) * temp_vec[i + j];
            }
        }

        bit_map[pos] = cnt;
        cnt = 0;
        pos++;
    }
    
    bit_map[0] = 1 & ((1 << 8) - 1);
    bit_map[1] = (1 >> 8) & ((1 << 8) - 1);
}

// Returns word offset of hole selected by the best fit memory allocation algorithm, and -1 if there is no fit
int bestFit(int sizeInWords, void *list) {
    int diff = INT16_MAX;
    int offset = -1;
    auto* holeList = static_cast<uint16_t*>(list);
    uint16_t length = *holeList++;
    
    for(uint16_t i = 1; i < (length) * 2; i += 2) {
        if(holeList[i] >= sizeInWords && holeList[i] < diff) {
            offset = holeList[i-1];
            diff = holeList[i];
        }
    }
    
return offset;
}

// Returns word offset of hole selected by the worst fit memory allocation algorithm, and -1 if there is no fit.
int worstFit(int sizeInWords, void *list) {
    int diff = -1;
    int offset = -1;
    auto* holeList = static_cast<uint16_t*>(list);
    uint16_t length = *holeList++;

    for(uint16_t i = 1; i < (length) * 2; i += 2) {
        if(holeList[i] >= sizeInWords && holeList[i] > diff) {
            offset = holeList[i-1];
            diff = holeList[i];
        }
    }
return offset;
}


void MemoryManager::combine_holes() {}
