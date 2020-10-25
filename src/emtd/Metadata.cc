/*
 *	EMTD Project
 *	Metadata Class defines methods to initialize metadata from file and maintain it
 *
 *	Version:	Aug 28, 2017
 *	Author:		Mark Gallagher (markgall@umich.edu)
 *
 *  Modified for gem5 by Zelalem Aweke (zaweke@umich.edu)
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <map>
#include <fstream>
#include <iostream>

#include "emtd/Metadata.hh"
#include "debug/emtd.hh"
#include "debug/emtd_warning.hh"
#include "debug/priv.hh"
#include "debug/csd.hh"

Metadata::Metadata(MetadataParams *params) : SimObject(params), filename(params->filename), insfilename(params->insfilename),  
    progname(params->progname), clock_period(params->clock), enc_latency(params->enc_latency)
{
    // Do some error checking on this path: See it exists
    if (access(filename.c_str(), F_OK) != 0)
    {
        //throw std::runtime_error("Could not open " + filename);
    }
    // Init mask for variable tag size
    assert(EMTD_TAG_BIT_WIDTH + EMTD_TAG_TYPE_BIT_WIDTH <= 8);

    // Initialize the tag-parsing masks
    tag_bits_mask = 0x00;
    tag_type_bits_mask = 0x00;
    for (int bit = 0; bit < (EMTD_TAG_BIT_WIDTH + EMTD_TAG_TYPE_BIT_WIDTH); bit++){
        // Tag bits in the LSBs
        if (bit < EMTD_TAG_BIT_WIDTH){
            tag_bits_mask |= (1 << bit);
        }
        // Type-bits in the part after the tag bits
        else if (bit >= EMTD_TAG_BIT_WIDTH){
            tag_type_bits_mask |= (1 << bit);
        }
    }

    // Load the initial metadata info from the supplied files
    //load_metadata_binary(filename.c_str());
    load_ins_taints(insfilename.c_str());

    // Initialize register tags
    initialize_reg_tags();
}



void Metadata::initialize_reg_tags()
{
    // init all reg tags status to CLEAN
    for (int idx = 0; idx < 32; idx++){
        RegId reg(IntRegClass, idx);
        reg_tags_status.insert(std::pair<RegId, Emtd_status_tag>(reg, CLEAN));
    }

    //x0 (zero reg) is data
    RegId reg0(IntRegClass, 0);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg0, DATA));

    // x1 (ra) is code pointer
    RegId reg1(IntRegClass, 1);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg1, DATA));

    // x2 = sp, x3 = gp, x4 = tp
    for (int idx = 2; idx < 5; idx++){
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }

    // x5 - x7
    for (int idx = 5; idx < 8; idx++){
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }

    // x8 (fp) is data pointer
    RegId reg8(IntRegClass, 8);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg8, DATA));

    //x9 - x31
    for (int idx = 9; idx < 32; idx++){
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }
}




/******************************************************************/
//  Helper functions for tag management
/******************************************************************/
// Get a tag for an addr. Assume DATA if non-existant
Emtd_tag Metadata::get_mem_tag(memaddr_t memaddr){
    // If address exists, return tag
    if (memory_tags.count(memaddr) > 0){
        return memory_tags[memaddr];
    }
    // If it doesn't exist...see if a tag at the 8-byte boundary exists
    else if (memory_tags.count(memaddr - (memaddr % EMTD_DATA_TAG_GRANULARITY)) > 0){
        return memory_tags[memaddr - (memaddr % EMTD_DATA_TAG_GRANULARITY)];
    }
    // Otherwise return default 
    else {
        return DATA;
    }
}

// Set the tag for a memory address
void Metadata::set_mem_tag(memaddr_t memaddr, Emtd_tag newtag, Addr pc){
    // Only write in at DATA_TAG_GRANULARITY aligned addresses
    // This isn't a problem for CODE_TAG_GRANULARITY since code tags aren't set again after load time
    memaddr_t addrToTag = memaddr;
    if (memaddr % EMTD_DATA_TAG_GRANULARITY != 0){
        addrToTag -= memaddr % EMTD_DATA_TAG_GRANULARITY;
    }
    DPRINTF(emtd, "0x%x: Wrote tag %s to memory 0x%x\n", pc, EMTD_TAG_NAMES[newtag], memaddr);

    // Overwrite the tag at this tag address
    // To save on space, only track live ciphertexts! 
    auto it = memory_tags.find(addrToTag);
    if (it != memory_tags.end()){
        // Tag entry exists! 
        Emtd_tag oldtag = memory_tags[addrToTag];
        if(newtag==CIPHERTEXT){
            memory_tags[addrToTag] = newtag;
            DPRINTF(priv, "MEM 0x%x :: Changing memory tag from %s to %s\n", addrToTag, EMTD_TAG_NAMES[oldtag], EMTD_TAG_NAMES[newtag]);
        }
        else{
            memory_tags.erase(addrToTag);
            DPRINTF(priv, "MEM 0x%x :: Clearing memory tag (was %s, overwritten by %s)\n", addrToTag, EMTD_TAG_NAMES[oldtag], EMTD_TAG_NAMES[newtag]);
        }
    }
    else {
        if(newtag==CIPHERTEXT){
            memory_tags[addrToTag] = newtag;
            DPRINTF(priv, "MEM 0x%x :: Adding memory tag %s\n", addrToTag, EMTD_TAG_NAMES[newtag]);
        }
    }
}

// Get a tag for an insn const. If non-existant, give UNTAGGED (non-constant loading insn)
// Emtd_tag Metadata::get_insn_const_tag(memaddr_t memaddr){
//     // See if the memory address exists
//     if (insns_consts_tags.count(memaddr) > 0)
//     {
//         return insns_consts_tags[memaddr];
//     }
//     return UNTAGGED;
// }

// Get a tag for register
Emtd_tag Metadata::get_reg_tag(RegId regIdx){

    if (reg_tags.count(regIdx) > 0){
        return reg_tags[regIdx];
    }
    return UNTAGGED;
}

// Set tag for register
void Metadata::set_reg_tag(RegId regIdx, Emtd_tag newtag, Addr pc){
    if (!regIdx.isZeroReg()){
        reg_tags[regIdx] = newtag;
    }
    if (newtag == CIPHERTEXT){
        DPRINTF(priv, "REG :: R%d tagged as %s\n", regIdx.flatIndex(), EMTD_TAG_NAMES[newtag]);
    }
    DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[newtag], regIdx.index());
}

// Get a status tag for register
Emtd_status_tag Metadata::get_reg_tag_status(RegId regIdx){
    if (reg_tags_status.count(regIdx) > 0){
        return reg_tags_status[regIdx];
    }
    return AHHH;
}

// Set status tag for register
void Metadata::set_reg_tag_status(RegId regIdx, Emtd_status_tag tag){
    if (!regIdx.isZeroReg()){
        reg_tags_status[regIdx] = tag;
    }
}

// Clear a range of tags in memory
void Metadata::clear_mem_range_tags(memaddr_t bound1, memaddr_t bound2){
    memaddr_t lower_bound = (bound1 < bound2) ? bound1 : bound2;
    memaddr_t upper_bound = (lower_bound == bound1) ? bound2 : bound1;

    // Declare iterators for the map. These point to the start and end elements in the stack
    auto lower_it = memory_tags.lower_bound(lower_bound);
    auto upper_it = memory_tags.upper_bound(upper_bound);

    // Clear the range
    memory_tags.erase(lower_it, upper_it);
}

// Get effective memory address for a ld/st operation
memaddr_t Metadata::get_mem_addr(StaticInstPtr inst, Trace::InstRecord *traceData){
    assert(traceData);
    return traceData->getAddr();
}
/******************************************************************/
//  END: Helper functions for tag management
/******************************************************************/




/******************************************************************/
//  Helper functions for loading the metadata binary
/******************************************************************/
// Convert a byte array of 8 elements into a 64-bit memory address
memaddr_t Metadata::convert_byte_array_to_addr(uint8_t *byte_array){
    memaddr_t result =
        static_cast<memaddr_t>(byte_array[0]) |
        static_cast<memaddr_t>(byte_array[1]) << 8 |
        static_cast<memaddr_t>(byte_array[2]) << 16 |
        static_cast<memaddr_t>(byte_array[3]) << 24 |
        static_cast<memaddr_t>(byte_array[4]) << 32 |
        static_cast<memaddr_t>(byte_array[5]) << 40 |
        static_cast<memaddr_t>(byte_array[6]) << 48 |
        static_cast<memaddr_t>(byte_array[7]) << 56;

    return result;
}

// Current implementation: Tag stored in lower bits of tagbyte
Emtd_tag Metadata::convert_tagbyte_to_tag(uint8_t tagbyte){
    return static_cast<Emtd_tag>(tagbyte & tag_bits_mask);
}

// Current implementation: Tag type stored in upper bits
Emtd_tag_type Metadata::convert_tagbyte_to_tag_type(uint8_t tagbyte){
    return static_cast<Emtd_tag_type>((tagbyte & tag_type_bits_mask) >> EMTD_TAG_BIT_WIDTH);
}
/******************************************************************/
//  END: Helper functions for loading the metadata binary
/******************************************************************/




/******************************************************************/
//  Helper functions for stack tag management
/******************************************************************/
// check and clear tags for deallocated stack
void Metadata::check_stack_pointer(ThreadContext *tc){
    if (!max_sp.empty() && max_sp.back() > tc->readIntReg(2)){
        max_sp.back() = tc->readIntReg(2);
    }
}

void Metadata::save_sp(ThreadContext *tc){
    base_sp.push_back(tc->readIntReg(2));
    max_sp.push_back(tc->readIntReg(2));
}

void Metadata::deallocate_stack_tags(){
    // You are returning from a function and you need to dealloc
    if (base_sp.back() > max_sp.back()){
        clear_mem_range_tags(max_sp.back(), base_sp.back() - 8);
        DPRINTF(emtd, "Cleared stack from 0x%x to 0x%x\n", max_sp.back(), base_sp.back() - 8);
    }
    max_sp.pop_back();
    base_sp.pop_back();
}
/******************************************************************/
//  END: Helper functions for stack tag management
/******************************************************************/




/******************************************************************/
//  Helper functions for shadow register encryption
/******************************************************************/
uint64_t Metadata::get_enc_latency(){
    return enc_latency;
}

uint64_t Metadata::get_reg_update_time_cycles(RegId regIdx, bool is_fp_op){

    //Returns how many cycles ago the last reg update was
    if (is_fp_op){
        if (fp_reg_updates_ticks.find(regIdx.index()) != fp_reg_updates_ticks.end()){
            uint64_t elapsed_ticks = curTick() - fp_reg_updates_ticks[regIdx.index()];
	        //uint64_t elapsed_cycles = divCeil(elapsed_ticks, clock_period);
	        uint64_t elapsed_cycles = (elapsed_ticks + clock_period - 1) / clock_period;
            // DPRINTF(csd, "\t\t GOT FP R%d update time, last ticks :: %d\n ", regIdx.index(), fp_reg_updates_ticks[regIdx.index()]); 
            // DPRINTF(csd, "\t\t GOT FP Reg update time, elapsed cycles :: %d\n ", elapsed_cycles); 
            return elapsed_cycles;
        }
    }
    else {
        if (int_reg_updates_ticks.find(regIdx.index()) != int_reg_updates_ticks.end()){
            uint64_t elapsed_ticks = curTick() - int_reg_updates_ticks[regIdx.index()];
            //int64_t elapsed_cycles = divCeil(elapsed_ticks, clock_period);
            uint64_t elapsed_cycles = (elapsed_ticks + clock_period - 1) / clock_period;
            // DPRINTF(csd, "\t\t GOT INT R%d update time, last ticks :: %d\n ", regIdx.index(), int_reg_updates_ticks[regIdx.index()]); 
            // DPRINTF(csd, "\t\t INT Reg update time, elapsed cycles :: %d\n ", elapsed_cycles); 
            return elapsed_cycles;
        }
    }
    return 0;
}


// Set status tag for register
void Metadata::record_reg_update(RegId regIdx, bool is_fp_op, bool is_tainted, bool is_load){
    // If insn is not tainted, no delay for store path, shadow register is duplicate
    // If load, delay for store path is modeled by dec injection, so no added delay here (load followed by immediate store)
    uint64_t update_time = (is_load || !is_tainted) ? 1 : curTick();

    if (!regIdx.isZeroReg()){
        if (is_fp_op){
            //Debug prints
            // if(fp_reg_updates_ticks.find(regIdx.index()) != fp_reg_updates_ticks.end()){
            // if(update_time != fp_reg_updates_ticks[regIdx.index()]){ DPRINTF(csd, "\t\t SET FP R%d update time, last ticks :: %d\n ", regIdx.index(), update_time); }
            // } else { DPRINTF(csd, "\t\t INIT FP R%d update time, last ticks :: %d\n ", regIdx.index(), update_time); }
            //End debug prints
            fp_reg_updates_ticks[regIdx.index()] = update_time;
        }
        else {
            //Debug prints
            // if(int_reg_updates_ticks.find(regIdx.index()) != int_reg_updates_ticks.end()){
            // if(update_time != int_reg_updates_ticks[regIdx.index()]){ DPRINTF(csd, "\t\t SET FP R%d update time, last ticks :: %d\n ", regIdx.index(), int_reg_updates_ticks[regIdx.index()]); }
            // } else { DPRINTF(csd, "\t\t INIT FP R%d update time, last ticks :: %d\n ", regIdx.index(), int_reg_updates_ticks[regIdx.index()]); }
            //End debug prints
            int_reg_updates_ticks[regIdx.index()] = update_time;
        }
    }
}

void Metadata::void_reg_update(RegId regIdx, bool is_fp_op){

    if (!regIdx.isZeroReg()){
        if (is_fp_op){
            fp_reg_updates_ticks[regIdx.index()] = 1;
        }
        else {
            int_reg_updates_ticks[regIdx.index()] = 1;
        }
    }
}

/******************************************************************/
//  END: Helper functions for shadow register encryption
/******************************************************************/




/******************************************************************/
//  Helper functions for shadow cache/cam
/******************************************************************/

//Returns line in cache
int	    Metadata::get_shadow_cache_line_no(uint64_t counter){
    int line_no = 1;
    assert(line_no < CACHE_LINES);
    return line_no;
}	


//Adds counter to Cache
void    Metadata::update_shadow_cache(uint64_t counter){
    //int line_no= get_shadow_cache_line_no(counter);
    //Do something
} 	


//Adds counter to CAM
void    Metadata::update_shadow_cam(uint64_t counter){}


//Checks if counter is in Cache
bool    Metadata::index_shadow_cache(uint64_t counter){
    int line_no= get_shadow_cache_line_no(counter);
    for(int i=0; i<CACHE_WAYS; i++){
        if(shadow_cache[line_no][i] == counter){
            return true;
        }
    }
    return false;
}


//Checks if counter is in CAM
bool    Metadata::index_shadow_cam(uint64_t counter){ return false; }


bool    Metadata::access_shadow_cache(memaddr_t eff_addr){
    //Retrieves counter from memory_counters, indexes/updates Cache using counter, Returns hit
    if (memory_counters.find(eff_addr) != memory_counters.end()){
        uint64_t counter = memory_counters[eff_addr];
        bool counter_present = index_shadow_cache(counter);
        if(counter_present){
            // DO something? Update LRU chain?
            return true;
        }
        else{
            // Update Cache?? Or not until later??
            return false;
        }
    }
    else{
        DPRINTF(csd, "WARNING:: Effective Address 0x%x is not tracked in MemoryCounters::\n ", eff_addr);
        return false;
    }
}


//Retrieves counter from memory_counters, indexes/updates CAM using counter, Returns hit
bool    Metadata::access_shadow_cam(memaddr_t eff_addr){ return false; }
    
/******************************************************************/
//  END: Helper functions for shadow cache/cam
/******************************************************************/












// Populates both the memory_tags and insns_consts_tags maps
void Metadata::load_metadata_binary(const char *filename){

    DPRINTF(priv, "Loading metadata binary...\n");
    int metafile_descriptor = open(filename, O_RDONLY);

    // Get file statistics (for its size)
    struct stat metafile_stats;
    assert(metafile_descriptor != -1);
    assert(fstat(metafile_descriptor, &metafile_stats) >= 0);
    size_t metafile_size = metafile_stats.st_size;

    // Perform mapping of this file's data to this process's virtual addr space
    char *buf = (char *)mmap(NULL, metafile_size, PROT_READ, MAP_PRIVATE, metafile_descriptor, 0);
    assert(buf != MAP_FAILED);
    close(metafile_descriptor);

    // Get the number of metadata entries in the file
    size_t num_meta_entries = metafile_size / sizeof(Emtd_MetadataEntry);
    assert(num_meta_entries > 0);
    DPRINTF(priv, "Found %d metadata entries...\n", num_meta_entries);

    // Parse through the metadata file
    auto meta_entries = (Emtd_MetadataEntry *)buf;
    for (unsigned i = 0; i < num_meta_entries; i++){
        // Get entry's starting address and tag
        memaddr_t entry_start_addr = convert_byte_array_to_addr(meta_entries[i].startaddr);
        Emtd_tag entry_tag = convert_tagbyte_to_tag(meta_entries[i].tagbyte);

        if (entry_tag == CIPHERTEXT){
            // If it's a ciphertext tag, it's a start addr
            // Assert the next entry is the ciphertexts' end addr
            assert(convert_tagbyte_to_tag(meta_entries[i + 1].tagbyte) == CIPHERTEXT);
            memaddr_t entry_end_addr = convert_byte_array_to_addr(meta_entries[i + 1].startaddr);

            // Insert the first code element into *memory_tags* map
            memory_tags[entry_start_addr] = entry_tag;
            DPRINTF(priv, "Tagging global variable at 0x%x with tag %s\n", entry_start_addr, EMTD_TAG_NAMES[entry_tag]);

            // Populate rest of the code entries
            for (memaddr_t idx = entry_start_addr; idx < entry_end_addr; idx += EMTD_CODE_TAG_GRANULARITY){
                memory_tags[idx] = entry_tag;
                DPRINTF(priv, "Tagging global variable at 0x%x with tag %s\n", idx, EMTD_TAG_NAMES[entry_tag]);
            }

            // Increment loop to skip next entry (already processed as the code end tag)
            i++;
        }
        else {
            // Any other entries must be data, otherwise issue with metadata file! 
            assert(entry_tag == DATA);
        }
    }
    munmap(buf, metafile_size);
}

// Populates insn_tags
void Metadata::load_ins_taints(const char *filename){
    DPRINTF(priv, "Loading instruction taints... %s\n", filename);
    std::ifstream ins_taints (filename);
    assert(ins_taints.is_open());

    std::string line;
    while(getline(ins_taints, line)){
        //Get first arg, insn addr
        int pos = line.find("; ");
        long addr = strtol(line.substr(0, pos).c_str(), NULL, 16);
        line.erase(0, pos + 2);

        //Get second arg, is arith tainted
        pos = line.find("; ");
        int arith_tainted = strtol(line.substr(0, pos).c_str(), NULL, 10);
        line.erase(0, pos + 2);

        //Get third arg, is memory access tainted
        pos = line.find("; ");
        int mem_tainted = strtol(line.substr(0, pos).c_str(), NULL, 10);
        line.erase(0, pos + 2);

	insn_tags[addr] = Emtd_InsnTaintEntry(addr, (bool)arith_tainted, (bool)mem_tainted);
	DPRINTF(priv, "Tainting instruction at 0x%x , %d, %d\n", addr, arith_tainted, mem_tainted);
    }
    ins_taints.close();
}

bool Metadata::isTainted(Addr pc){
    auto it = insn_tags.find(pc);
    if (it != insn_tags.end()){
       //fprintf(stderr, "tainted pc = %x current pc = %x \n",(unsigned int)value, (unsigned int)pc.pc());
       return true;
    }    
    return false;
}

Emtd_InsnTaintEntry Metadata::getInsnTaintEntry(Addr pc){
    auto it = insn_tags.find(pc);
    if (it != insn_tags.end()){
       return insn_tags[pc];
    }    
    return Emtd_InsnTaintEntry();
}


void Metadata::commit_insn(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData){

    X86Ops Ops;
    std::string opc = inst->getName();
    Emtd_InsnTaintEntry taints = getInsnTaintEntry(pc);

    bool is_tainted = false;
    if(taints.arith_tainted || taints.mem_tainted){
        DPRINTF(csd, "Committing Tainted Instruction 0x%x :: %s\n", pc, inst->generateDisassembly(pc, NULL));
        is_tainted = true; 
    }

    try
    {
		std::__cxx11::string diss = inst->generateDisassembly(0, NULL);

        /*** Enc/Dec ***/
		if((diss.find("enc") != std::string::npos) || (diss.find("dec") != std::string::npos)){
            // Do not update for enc/dec instructions
            return;
        }
    
        /*** Loads/Stores ***/
        else if (inst->isMemRef())
        {
            Addr eff_addr = get_mem_addr(inst, traceData);

            if (inst->isLoad()){
                if(diss.find(" t") != std::string::npos){
                    if(is_tainted) { DPRINTF(csd, "WARNING:: IGNORING Reg Update for TEMP register in Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL)); }
                    return;
                }

                if(diss.find("ldfp ") != std::string::npos){
                    void_reg_update(RD, true);   // if(is_tainted) { DPRINTF(csd, "Recording FP Reg Update for Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL)); }
                }
                else if(diss.find("ld ") != std::string::npos){
                    void_reg_update(RD, false);  // if(is_tainted) { DPRINTF(csd, "Recording INT Reg Update for Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL)); }
                }
                else if (is_tainted){
                    DPRINTF(csd, "WARNING:: LOAD is not FP or INT: Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL));
                    return;
                }
            }
            else if (inst->isStore()){
                // Add eff_addr to memory_counters
                memory_counters[eff_addr] = global_counter; 
                // Increment coutner
                global_counter++; 
                // Add counter to cache
                update_shadow_cache(memory_counters[eff_addr]);
                return;
            }
            else {
                DPRINTF(priv, "WARNING:: Unhandled mem ref case\n");
                return;
            }
            
        }

        /*** Jumps ***/
        else if (inst->isControl()){     
            return;
        }

        /*** Branches ***/
        else if (Ops.is_branch_op(opc)){
            return;
        }

        /*** REG Arithmetic ***/
        else {
            if(inst->isInteger() && !inst->isFloating()){
                record_reg_update(RD, inst->isFloating(), is_tainted, false); // if(is_tainted) { DPRINTF(csd, "\t\t Recording INT Reg Update for Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL)); }
            }
            else if(!inst->isInteger() && inst->isFloating()){
                record_reg_update(RD, inst->isFloating(), is_tainted, false); // if(is_tainted) { DPRINTF(csd, "\t\t Recording FP Reg Update for Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL)); }
            }
            else if (is_tainted) {
                DPRINTF(csd, "WARNING:: Insn is not FP or INT: Tainted Instruction 0x%x :: %s\n ", pc, inst->generateDisassembly(pc, NULL));
                return;
            }

        }
    }
    catch (int e)
    {
        exit(e);
    }

}


void Metadata::propagate_result_tag_o3(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData){

    X86Ops Ops;
    std::string opc = inst->getName();

    if(isTainted(pc)){
        DPRINTF(csd, "Committing Tainted Instruction 0x%x :: %s\n", pc, inst->generateDisassembly(pc, NULL));
    }

    try
    {
        /*** Loads: Take the tag from memory and override the RD tag
        **** Invalid Op: Address being used (RS1) is a ciphertext type
        ***/
        /*** Stores: Take the tag of RS2 (reg being stored) and override the tag in memory
        **** Invalid Op: Address being used (RS1) is a ciphertext type 
        ***/
        if (inst->isMemRef())
        {
            // Assert effective address is not tagged as ciphertext
            Addr eff_addr = get_mem_addr(inst, traceData);
            Emtd_tag eff_addr_tag = get_reg_tag(RS1);
            if (eff_addr_tag == CIPHERTEXT){
                DPRINTF(priv, "PANIC:: Policy violated on ld/st, effective address is ciphertext\n");
            }

            // Propagate destination tag
            if (inst->isLoad()){
                Emtd_tag rd_tag = get_mem_tag(eff_addr);
                set_reg_tag(RD, rd_tag, pc);
                if(rd_tag == CIPHERTEXT){
                    DPRINTF(priv, "OP :: LOAD from 0x%x with tag %s\n", eff_addr, EMTD_TAG_NAMES[rd_tag]);
                }
            }
            else if (inst->isStore()){
                Emtd_tag rs2_tag = get_reg_tag((RS2));
                set_mem_tag(eff_addr, rs2_tag, pc);
                if(rs2_tag == CIPHERTEXT){
                    DPRINTF(priv, "OP :: Store to 0x%x with tag %s\n", eff_addr, EMTD_TAG_NAMES[rs2_tag]);
                }
            }
            else{
                // TODO PANIC
                DPRINTF(priv, "PANIC:: Unhandled mem ref case\n");
            }
            
        }

        /*** Jumps: Move tag of NPC into RD (saves a return addr)
        **** Invalid Op: Address being used (RS1 for JALR only...) is not a CODE_PTR
        ***/
        /*** Branches: Nothing happens here. The resulting PC is made by get_next_pc_tag later in execution
        **** Invalid Op: Using CODE as an operand
        ***/
        else if (inst->isControl()){
        
            if(inst->isUncondCtrl()){

                // Assert target address is not tagged as ciphertext
                Emtd_tag target_addr_tag = get_reg_tag(RS1);
                if (target_addr_tag == CIPHERTEXT){
                    DPRINTF(priv, "PANIC:: Policy violated on control transfer, target address is ciphertext\n");
                }

                // Propagate destination tag
                if (inst->isCall()){
                    // STACK FRAM HANDLING CAUSING ERRORS
                    //save_sp(tc);
                }
                else if (inst->isReturn()){
                    // STACK FRAM HANDLING CAUSING ERRORS
                    //deallocate_stack_tags();
                }

                // Set tag of destination register
                if (RD.index() != 0)
                    set_reg_tag(RD, DATA, pc);

            }
            else if (Ops.is_jump_op(opc)){
                //Jump op that isn;t unconditional... weird
                //Still do this 
                if (RD.index() != 0)
                    set_reg_tag(RD, DATA, pc);
                set_reg_tag_status(RD, CLEAN);
                //Figure out what's going on here... TODO 
                std::cerr << "jump_op :: ";
                inst->printFlags(std::cerr, "; ");
                std::cerr << "\n";

            }
            else if (inst->isCondCtrl()){

            }
            else {
                //Some other case here idk what it is... but showed up in bmk 
                //DPRINTF(priv, "PANIC:: Unhandled control case\n"); TOO MUCH PRINTING D:<

            }

        }

        /*** Branches: Nothing happens here. The resulting PC is made by get_next_pc_tag later in execution
        **** Invalid Op: Using CODE as an operand
        ***/
        else if (Ops.is_branch_op(opc))
        {
            // Invalid using CODE as an operand
            DPRINTF(priv, "PANIC:: Unhandled branch case\n");
        }





        /*** OTHER:: TODO!!
        ****
        ***/
        else {

            /*** REG Arithmetic: Check tags of RS1 and RS2 for RD tag
            **** Invalid Ops: Check the header file. This depends on the source tags
            ***/

            Emtd_tag rs1_tag = get_reg_tag(RS1);
            Emtd_tag rs2_tag = get_reg_tag(RS2);
            if (rs1_tag == CIPHERTEXT || rs2_tag == CIPHERTEXT){
                DPRINTF(priv, "OP :: R%d <-- R%d op R%d with tag %s\n", RD.index(), RS1.index(), RS2.index(), EMTD_TAG_NAMES[CIPHERTEXT]);
            }

            if (Ops.is_reg_arith_op(opc) || Ops.is_cmp_op(opc))
            {
                // Check Invalid Op
                // TODO

                if (get_reg_tag(RS1) == CIPHERTEXT || get_reg_tag(RS2) == CIPHERTEXT){
                    set_reg_tag(RD, CIPHERTEXT, pc);
                    // set_reg_tag_status(RD, CLEAN, pc);
                    DPRINTF(priv, "OP :: R%d <-- R%d op R%d with tag %s\n", RD.index(), RS1.index(), RS2.index(), EMTD_TAG_NAMES[CIPHERTEXT]);
                }
                else {
                    set_reg_tag(RD, DATA, pc);
                    // set_reg_tag_status(RD, CLEAN);
                }

                //check if the stack pointer is changing
                // check_stack_pointer(tc);
            }

            /*** IMMED Arithmetic: Moves the tag of RS1 into RD
            **** Invalid Op: Still can't use CODE as an operand
            ***/
            else if (Ops.is_immed_arith_op(opc))
            {
                // Check Invalid Op
                // TODO

                set_reg_tag(RD, get_reg_tag(RS1), pc);
                // set_reg_tag_status(RD, get_reg_tag_status(RS1));
                
                //check if the stack pointer is changing
                // check_stack_pointer(tc);
            }

            /*** All Else: If there is an RD (not the ZERO register), assign a DATA tag
            ***/
            else
            {
                set_reg_tag(RD, DATA, pc);
                set_reg_tag_status(RD, CLEAN);
            }
        }
    }
    catch (int e)
    {
        exit(e);
    }
}




/******************************************************************/
//  Deprecated functions for recording stats
/******************************************************************/
// void Metadata::write_violation_stats()
// {
//     DPRINTF(emtd_warning, "Dumping stats file\n");
//     std::ofstream file;
//     file.open("m5out/violation_stats.txt");

//     std::map<Addr, int>::iterator it = pc_violation_counts.begin();

//     while (it != pc_violation_counts.end())
//     {
//         Addr pc = it->first;
//         int count = it->second;
//         std::string warning = pc_violation_type[pc];
//         //file << std::to_string(pc) + ", " + std::to_string(count) + ", " + warning + " \n";
//         file << "0x" << std::hex << pc << ", " + std::to_string(count) + ", " + warning + " \n";

//         it++;
//     }
//     file.close();
// }

// void Metadata::record_violation(Addr pc, std::string msg, std::string pc_msg)
// {
//     if ((int)pc < (int)libc_start)
//     {
//         // Warning is from the program, not library code
//         DPRINTF(emtd_warning, "%s", pc_msg);
//         DPRINTF(emtd_warning, "%s", msg);
//         warning_count++;
//     }
//     if (pc_violation_counts.count(pc) == 0)
//     {
//         // First time we've encountered this warning
//         pc_violation_counts[pc] = 1;
//         pc_violation_type[pc] = msg;
//     }
//     else
//     {
//         pc_violation_counts[pc]++;
//     }
// }
/******************************************************************/
//  END: Deprecated functions for recording stats
/******************************************************************/



Metadata *
MetadataParams::create()
{
    return new Metadata(this);
}
