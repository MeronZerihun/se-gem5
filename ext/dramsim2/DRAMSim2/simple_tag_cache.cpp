
#include "simple_tag_cache.h"

SimpleTagCache::SimpleTagCache(){
}

unsigned SimpleTagCache::access(uint64_t address, uint64_t current_cycle){

    //process pending misses first
    
    if(!miss_response_time.empty()){
        for(int i=0; i< miss_response_time.size(); i++){
            if(miss_response_time[i] <= current_cycle){
               lru_block[address]  = current_cycle;
               miss_response_time.erase(miss_response_time.begin() + i );
               miss_address.erase( miss_address.begin() + i );
               break; //handle one response at a time
            }
        }
    }

    //evict something if we have exceeded the size
    if(lru_block.size() > TAG_CACHE_SIZE){

        //find the LRU block
        uint64_t min = current_cycle;
        uint64_t block_addr=0;
        for (std::pair<uint64_t, uint64_t> block : lru_block){
            if(min > block.second ){
                min = block.second;
                block_addr = block.first;
            } 
        }
        

        //remove LRU block
        lru_block.erase(block_addr) ; 


    }

    //check if it is a hit
    bool hit =  (lru_block.count(address) !=0 );
    if(hit){
        //update LRU for the block we just accessed    
        lru_block[address]  = current_cycle;

        return 0;
    }

    else {
        //check if miss already being processed
        for(int i=0; i< miss_address.size(); i++){
            if(miss_address[i] == address){
                return miss_response_time[i] - current_cycle;
            }
        }

        miss_address.push_back(address);
        miss_response_time.push_back(current_cycle + TAG_MISS_DELAY);
        return current_cycle + TAG_MISS_DELAY;
    }
  
}