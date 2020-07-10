

#ifndef SIMPLE_FA_CACHE_H
#define SIMPLE_FA_CACHE_H

class SimpleTagCache; //to handle circular includes


#include <vector>
#include <algorithm>
#include <cassert>
#include <unordered_map>


#include "util.h"

using namespace std;

#define TAG_CACHE_SIZE 4096
#define TAG_MISS_DELAY 50


class SimpleTagCache{

public:
    //memory system to pass misses to
    SimpleTagCache();
    unsigned access(uint64_t, uint64_t);
    
private:
    
    uint64_t current_cycle;

    //address and last accessed cycle
    unordered_map<uint64_t, uint64_t> lru_block;
    
    //pending queue: address and when it shall be ready
    vector<uint64_t> miss_address;
    vector<uint64_t> miss_response_time;

    unordered_map<uint64_t, uint64_t> misses;
    

};

#endif
