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

#include "emtd/Metadata.hh"
#include "debug/emtd.hh"
#include "debug/emtd_warning.hh"
#include "debug/priv.hh"


Metadata::Metadata(MetadataParams *params) : SimObject(params), filename(params->filename), progname(params->progname)
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

    // Load the initial metadata info from the supplied file
    load_metadata_binary(filename.c_str());

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
void Metadata::set_mem_tag(memaddr_t memaddr, Emtd_tag newtag){
    // Only write in at DATA_TAG_GRANULARITY aligned addresses
    // This isn't a problem for CODE_TAG_GRANULARITY since code tags aren't set again after load time
    memaddr_t addrToTag = memaddr;
    if (memaddr % EMTD_DATA_TAG_GRANULARITY != 0){
        addrToTag -= memaddr % EMTD_DATA_TAG_GRANULARITY;
    }

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
void Metadata::set_reg_tag(RegId regIdx, Emtd_tag newtag){
    if (!regIdx.isZeroReg()){
        reg_tags[regIdx] = newtag;
    }
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
    if (tag == CIPHERTEXT){
        DPRINTF(priv, "REG 0x%x :: Reg %d tagged as %s\n", regIdx.flatIndex(), EMTD_TAG_NAMES[newtag]);
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
            DPRINTF(priv, "Tagging global variable at 0x%x with tag %d\n", entry_start_addr, entry_tag);

            // Populate rest of the code entries
            for (memaddr_t idx = entry_start_addr; idx < entry_end_addr; idx += EMTD_CODE_TAG_GRANULARITY){
                memory_tags[idx] = entry_tag;
                DPRINTF(priv, "Tagging global variable at 0x%x with tag %d\n", idx, entry_tag);
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




void Metadata::propagate_result_tag_o3(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData){

    X86Ops Ops;
    std::string opc = inst->getName();

    try
    {
        char warn1[100];
        char warn2[100];

        // Propogation + Data-Oblivious Policies: 
        // LD:   Address cannot be ciphertext
        //       Propagate ciphertext tag to register
        // ST:   Address cannot be ciphertext
        //       Propagate ciphertext tag to memory
        // J/Br: Dest cannot be ciphertext
        //       No proagation rules 
        // DEF:  No limitations
        //       Always propagate ciphertext tag to dest reg


        /*if (RS1_TAG == CIPHERTEXT || RS2_TAG == CIPHERTEXT){
            DPRINTF(priv, "Source operands are ciphertexts\n");
        }*/

        /*** Loads: Take the tag from memory and override the RD tag
             Invalid Op: Address being used (RS1) is a ciphertext type
        ***/
        if (Ops.is_memory_load_op(opc))
        {
            // Write the resulting tag into RD
            Addr mem_addr = get_mem_addr(inst, traceData);
            Emtd_tag rd_tag = get_mem_tag(mem_addr);
            set_reg_tag(RD, rd_tag);

            if(rd_tag == CIPHERTEXT){
                DPRINTF(priv, "LOAD from 0x%x with tag %d\n", mem_addr, rd_tag);
		set_reg_tag(RD, rd_tag);
            }
            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[get_mem_tag(get_mem_addr(inst, traceData))], RD.index());
            DPRINTF(emtd, "ADDR LOADED FROM: 0x%x\n", get_mem_addr(inst, traceData));
        }

        /*** Stores: Take the tag of RS2 (reg being stored) and override the tag in memory
             Invalid Op: Address being used (RS1) is a ciphertext type 
        ***/
        else if (Ops.is_memory_store_op(opc))
        {
            // Write tag from RS2 into memory
            Addr mem_addr = get_mem_addr(inst, traceData);
            set_mem_tag(mem_addr, get_reg_tag(RS2));
            if (get_reg_tag(RS2) == CODE_PTR)
            {

            }

            DPRINTF(emtd, "0x%x: Wrote tag %s to memory 0x%x\n", pc, EMTD_TAG_NAMES[get_reg_tag(RS2)], get_mem_addr(inst, traceData));
        }

        /*** REG Arithmetic: Check tags of RS1 and RS2 for RD tag
                 Invalid Ops: Check the header file. This depends on the source tags
            ***/
        else if (Ops.is_reg_arith_op(opc))
        {
            // Outlawing any arithmetic on code pointers (this is actually C-standard)
            if (get_reg_tag(RS1) == CODE_PTR || get_reg_tag(RS2) == CODE_PTR)
            {
                sprintf(warn1, "0x%lu\n", pc);
                sprintf(warn2, "EMTD CHECK: Policy Violated on REG ARITH. Operand is CODE_PTR(3) %s.\n", opc.c_str());
                //DPRINTF(emtd_warning, "RS1 Tag: %s \tRS2 Tag: %s\n", EMTD_TAG_NAMES[RS1_TAG], EMTD_TAG_NAMES[RS2_TAG]);
                set_reg_tag(RD, DATA);
                set_reg_tag_status(RD, CLEAN);
                //throw EMTD_INVALIDOP;
                //record_violation(pc, std::string(warn2), std::string(warn1));
            }
            // All other ops should generate a DATA tag
            else
            {
                set_reg_tag(RD, DATA);
                set_reg_tag_status(RD, CLEAN);
                DPRINTF(emtd, "0x%x: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], RD.index());
            }
            //check if the stack pointer is changing
            //assert(d_inst->traceData);
            //check_stack_pointer(d_inst->traceData->getThread());
            check_stack_pointer(tc);
        }

        /*** IMMED Arithmetic: Moves the tag of RS1 into RD
                 Invalid Op: Still can't use CODE as an operand
            ***/
        else if (Ops.is_immed_arith_op(opc))
        {
            set_reg_tag(RD, get_reg_tag(RS1));
            set_reg_tag_status(RD, get_reg_tag_status(RS1));

            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[get_reg_tag(RS1)], RD.index());
            //check if the stack pointer is changing
            //assert(d_inst->traceData);
            //check_stack_pointer(d_inst->traceData->getThread());
            check_stack_pointer(tc);
        }

        /*** Compare operations: RD is always DATA
                 Invalid Ops: CODE better not be an operand... that's just wrong >_>
            ***/
        else if (Ops.is_cmp_op(opc))
        {
            // Restricting compares to within the same domain, BUT, compares to ZERO register are allowed
            // ALSO: immediate-operand based compares must be allowed.
            set_reg_tag(RD, DATA);
            set_reg_tag_status(RD, CLEAN);
            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], RD.index());
        }

        /*** Jumps: Move tag of NPC into RD (saves a return addr)
                 Invalid Op: Address being used (RS1 for JALR only...) is not a CODE_PTR
            ***/
        else if (Ops.is_jump_op(opc))
        {
            if (opc == "jalr" && get_reg_tag(RS1) != CODE_PTR)
            {
                sprintf(warn1, "0x%lu\n", pc);
                sprintf(warn2, "EMTD CHECK: Policy Violated on JALR. Tag should be CODE_PTR(3), got %s.\n", EMTD_TAG_NAMES[get_reg_tag(RS1)].c_str());
                //throw EMTD_INVALIDOP;
                //record_violation(pc, std::string(warn2), std::string(warn1));
            }

            // STACK FRAME HANDLING
            // Function Call? Save SP
            // CALLS == JAL/JALR's with RD == $ra
            if (RD.index() == 1)
            {
                //assert(d_inst->traceData);
                //save_sp(d_inst->traceData->getThread());
                save_sp(tc);
            }
            // Function Return? Deallocate
            else if (opc == "jalr" && inst->srcRegIdx(0).index() == 1 && RD.index() == 0)
            {
                deallocate_stack_tags();
            }
            // END STACK FRAME HANDLING

            //Write tag for next instruction if destination is not zero register
            if (RD.index() != 0)
                set_reg_tag(RD, CODE_PTR);
            set_reg_tag_status(RD, CLEAN);
        }

        /*** Branches: Nothing happens here. The resulting PC is made by get_next_pc_tag later in execution
                 Invalid Op: Using CODE as an operand
            ***/
        else if (Ops.is_branch_op(opc))
        {
            // Invalid using CODE as an operand
        }

        /*** All Else: If there is an RD (not the ZERO register), assign a DATA tag
            ***/
        else
        {
            set_reg_tag(RD, DATA);
            set_reg_tag_status(RD, CLEAN);
            DPRINTF(emtd, "0x%x: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], RD.index());
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
