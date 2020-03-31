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
//#include "emtd/churn/churn.hh"
#include "debug/emtd.hh"
#include "debug/emtd_warning.hh"
#include "debug/priv.hh"
//#include "debug/Churn.hh"

Metadata::Metadata(MetadataParams *params) : SimObject(params), filename(params->filename), progname(params->progname),
                                             threshold(0xffffffff), is_churning(false)
{
    // Do some error checking on this path: See it exists
    if (access(filename.c_str(), F_OK) != 0)
    {
        //throw std::runtime_error("Could not open " + filename);
    }
    // Init mask for variable tag size
    // First check for configuration error
    // TODO Confirm this works when tag bit width == 1
    assert(EMTD_TAG_BIT_WIDTH + EMTD_TAG_TYPE_BIT_WIDTH <= 8);
    // Initialize the tag-parsing masks
    tag_bits_mask = 0x00;
    tag_type_bits_mask = 0x00;
    for (int bit = 0; bit < (EMTD_TAG_BIT_WIDTH + EMTD_TAG_TYPE_BIT_WIDTH); bit++)
    {
        // Tag bits in the LSBs
        if (bit < EMTD_TAG_BIT_WIDTH)
        {
            tag_bits_mask |= (1 << bit);
        }
        // Type-bits in the part after the tag bits
        else if (bit >= EMTD_TAG_BIT_WIDTH)
        {
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
    for (int idx = 0; idx < 32; idx++)
    {
        RegId reg(IntRegClass, idx);
        reg_tags_status.insert(std::pair<RegId, Emtd_status_tag>(reg, DATA));
    }

    //x0 (zero reg) is data
    RegId reg0(IntRegClass, 0);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg0, DATA));

    // x1 (ra) is code pointer
    RegId reg1(IntRegClass, 1);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg1, DATA));

    // x2 = sp, x3 = gp, x4 = tp
    for (int idx = 2; idx < 5; idx++)
    {
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }

    // x5 - x7
    for (int idx = 5; idx < 8; idx++)
    {
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }

    // x8 (fp) is data pointer
    RegId reg8(IntRegClass, 8);
    reg_tags.insert(std::pair<RegId, Emtd_tag>(reg8, DATA));

    //x9 - x31
    for (int idx = 9; idx < 32; idx++)
    {
        RegId reg(IntRegClass, idx);
        reg_tags.insert(std::pair<RegId, Emtd_tag>(reg, DATA));
    }
}



void Metadata::inc_threshold(int inc)
{
    threshold = threshold + inc;
}
void Metadata::set_threshold(Addr new_threshold)
{
    threshold = new_threshold;
}
Addr Metadata::get_threshold()
{
    return threshold;
}
void Metadata::begin_churn()
{
    is_churning = true;
}
void Metadata::end_churn()
{
    is_churning = false;
}

// Get a tag for an addr. Assume UNTAGGED if non-existant
Emtd_tag Metadata::get_mem_tag(memaddr_t memaddr)
{
    // See if the memory address exists
    if (memory_tags.count(memaddr) > 0)
    {
        return memory_tags[memaddr];
    }
    // If it doesn't exist...see if a tag at the 8-byte boundary exists
    else
    {
        if (memory_tags.count(memaddr - (memaddr % EMTD_DATA_TAG_GRANULARITY)) > 0)
        {
            return memory_tags[memaddr - (memaddr % EMTD_DATA_TAG_GRANULARITY)];
        }
    }

    //return UNTAGGED;
    //Made defaut DATA to deal with input from files
    return DATA;
}

// Get a tag for an insn const. If non-existant, give UNTAGGED (non-constant loading insn)
Emtd_tag Metadata::get_insn_const_tag(memaddr_t memaddr)
{
    // See if the memory address exists
    if (insns_consts_tags.count(memaddr) > 0)
    {
        return insns_consts_tags[memaddr];
    }
    return UNTAGGED;
}

// Get a tag for register
Emtd_tag Metadata::get_reg_tag(RegId regIdx)
{

    if (reg_tags.count(regIdx) > 0)
    {
        return reg_tags[regIdx];
    }
    return UNTAGGED;
}

// Get a tag for register
Emtd_status_tag Metadata::get_reg_tag_status(RegId regIdx)
{

    if (reg_tags_status.count(regIdx) > 0)
    {
        return reg_tags_status[regIdx];
    }
    return AHHH;
}

// Set the tag for a memory address
void Metadata::set_mem_tag(memaddr_t memaddr, Emtd_tag newtag)
{
    // Only write in at DATA_TAG_GRANULARITY aligned addresses
    // This isn't a problem for CODE_TAG_GRANULARITY since code tags aren't set again after load time
    memaddr_t addrToTag = memaddr;
    if (memaddr % EMTD_DATA_TAG_GRANULARITY != 0)
    {
        addrToTag -= memaddr % EMTD_DATA_TAG_GRANULARITY;
    }

    // Overwrite the tag at this tag address
    memory_tags[addrToTag] = newtag;
}

// Set tag for register
void Metadata::set_reg_tag(RegId regIdx, Emtd_tag newtag)
{
    if (!regIdx.isZeroReg())
    {
        reg_tags[regIdx] = newtag;
    }
}

// Set tag for register
void Metadata::set_reg_tag_status(RegId regIdx, Emtd_status_tag tag)
{
    if (!regIdx.isZeroReg())
    {
        reg_tags_status[regIdx] = tag;
    }
}

// Clear a range of tags in memory
void Metadata::clear_mem_range_tags(memaddr_t bound1, memaddr_t bound2)
{
    memaddr_t lower_bound = (bound1 < bound2) ? bound1 : bound2;
    memaddr_t upper_bound = (lower_bound == bound1) ? bound2 : bound1;

    // Declare iterators for the map. These point to the start and end elements in the stack
    auto lower_it = memory_tags.lower_bound(lower_bound);
    auto upper_it = memory_tags.upper_bound(upper_bound);

    // Clear the range
    memory_tags.erase(lower_it, upper_it);
}

// Populates both the memory_tags and insns_consts_tags maps
void Metadata::load_metadata_binary(const char *filename)
{

    DPRINTF(priv, "Loading metadata binary...\n");

    // Open the metadata binary file
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
    for (unsigned i = 0; i < num_meta_entries; i++)
    {
        // Get entry's starting address and tag
        memaddr_t entry_start_addr = convert_byte_array_to_addr(meta_entries[i].startaddr);
        Emtd_tag entry_tag = convert_tagbyte_to_tag(meta_entries[i].tagbyte);
        //Emtd_tag_type entry_tag_type = convert_tagbyte_to_tag_type(meta_entries[i].tagbyte);

        if (entry_tag == CIPHERTEXT)
        {
            // If it's a ciphertext tag, it's a start addr
            // Assert the next entry is the ciphertexts' end addr
            assert(convert_tagbyte_to_tag(meta_entries[i + 1].tagbyte) == CIPHERTEXT);
            memaddr_t entry_end_addr = convert_byte_array_to_addr(meta_entries[i + 1].startaddr);

            // Insert the first code element into *memory_tags* map
            memory_tags[entry_start_addr] = entry_tag;
            DPRINTF(priv, "Tagging global variable at 0x%x with tag %d\n", entry_start_addr, entry_tag);

            // Populate rest of the code entries
            for (memaddr_t idx = entry_start_addr; idx < entry_end_addr; idx += EMTD_CODE_TAG_GRANULARITY)
            {
                memory_tags[idx] = entry_tag;
                DPRINTF(priv, "Tagging global variable at 0x%x with tag %d\n", idx, entry_tag);
            }

            // Increment loop to skip next entry (already processed as the code end tag)
            i++;
        }
        else
        {
            assert(entry_tag == DATA);
        }
        /*if (entry_tag_type == DATA_SEG) {
		    // Tag representing a global variable in the data segment. Add to the *memory_tags* map
		    DPRINTF(priv, "Found global variable at 0x%x in data segment with tag %d\n", entry_start_addr, entry_tag);
		    memory_tags[entry_start_addr] = entry_tag;
		} else if (entry_tag_type == INSN_CONST) {
		    // Tag representing the result of an instruction loading in a constant. Add to *insns_consts_tags* map
		    insns_consts_tags[entry_start_addr] = entry_tag;
		    DPRINTF(priv, "Found instruction loading a constant\n");	    
		}*/
    }

    // Unmap the metafile (clear memory, yay!)
    munmap(buf, metafile_size);
}

// Convert a byte array of 8 elements into a 64-bit memory address
memaddr_t Metadata::convert_byte_array_to_addr(uint8_t *byte_array)
{
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
Emtd_tag Metadata::convert_tagbyte_to_tag(uint8_t tagbyte)
{
    return static_cast<Emtd_tag>(tagbyte & tag_bits_mask);
}

// Current implementation: Tag type stored in upper bits
Emtd_tag_type Metadata::convert_tagbyte_to_tag_type(uint8_t tagbyte)
{
    return static_cast<Emtd_tag_type>((tagbyte & tag_type_bits_mask) >> EMTD_TAG_BIT_WIDTH);
}




void Metadata::propagate_result_tag_o3(ThreadContext *tc, StaticInstPtr inst, Addr pc, Trace::InstRecord *traceData)
{

    RISCVOps Ops;
    // Check for a valid rd_tag being set, meaning, we have an OVERRIDE TAG
    Emtd_tag rdTag = get_insn_const_tag(pc);
    if (rdTag != UNTAGGED)
    {
        // See if this is a store. If so, tag needs to go into the memory_tags map
        if (inst->isStore())
        {
            set_mem_tag(get_mem_addr_atomic(inst, traceData), rdTag);
        }
        else
        {
            RegId regIdx = inst->destRegIdx(0); //get destination register index
            set_reg_tag(regIdx, rdTag);
            set_reg_tag_status(regIdx, CLEAN);
        }
        // We are done here
        return;
    }

    // Save the $sp IF this is the insn coming *after* a function call
    // if (next_insn_save_sp) save_sp(inst);

    std::string opc = inst->getName();

    try
    {
        // So, not an insn loading a constant eh? Let's go through the policies then...
        char warn1[100];
        char warn2[100];

        /*if (RS1_TAG == CIPHERTEXT || RS2_TAG == CIPHERTEXT){
            DPRINTF(priv, "Source operands are ciphertexts\n");
        }*/

        /*** Loads: Take the tag from memory and override the RD tag
             Invalid Op: Address being used (RS1) is a non-ptr type
        ***/
        if (Ops.is_memory_load_op(opc))
        {
            // Write the resulting tag into RD
            Addr mem_addr = get_mem_addr_atomic(inst, traceData);
            Emtd_tag rd_tag = get_mem_tag(mem_addr);
            WRITE_RD_TAG_ATOMIC(rd_tag);

            //if( CIPHERTEXT){
            DPRINTF(priv, "LOAD from 0x%x with tag %d\n", mem_addr, rd_tag);
            //}

            if (rd_tag == CODE_PTR)
            {
                // Churn is working on reqAddr--reqAddr+BLOCK_SIZE
                // Clean below reqAddr, dirty above reqAddr
                if (!is_churning)
                {
                    WRITE_RD_STATUS_ATOMIC(CLEAN);
                }
                else if (mem_addr < threshold)
                {
                    WRITE_RD_STATUS_ATOMIC(CLEAN);
                }
                else
                {
                    WRITE_RD_STATUS_ATOMIC(STALE);
                }
            }
            else
            {
                WRITE_RD_STATUS_ATOMIC(CLEAN);
            }
            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[get_mem_tag(get_mem_addr_atomic(inst, traceData))], inst->destRegIdx(0).index());
            DPRINTF(emtd, "ADDR LOADED FROM: 0x%x\n", get_mem_addr_atomic(inst, traceData));
        }

        /*** Stores: Take the tag of RS2 (reg being stored) and override the tag in memory
                 Invalid Op: Address being used (RS1) is a non-ptr type OR value being stored is CODE
            ***/
        else if (Ops.is_memory_store_op(opc))
        {
            // Write tag from RS2 into memory
            Addr mem_addr = get_mem_addr_atomic(inst, traceData);
            WRITE_MEM_TAG_ATOMIC(mem_addr, RS2_TAG_ATOMIC);
            if (RS2_TAG_ATOMIC == CODE_PTR)
            {
                //Check threshold
                //PRIV
                /*if(!is_churning){
                    if (RRS2_STATUS_ATOMIC == STALE){
                        DPRINTF(Churn, "DDAS CHURN STATUS WARN:: Storing STALE pointer to CLEAN mem location, Churn DONE\n");
                    }
                }
                else {
                    if ((mem_addr < threshold) && RS2_STATUS_ATOMIC == STALE) {
                        DPRINTF(Churn, "DDAS CHURN STATUS WARN:: Storing STALE pointer to CLEAN mem location\n");

                    } else if ((mem_addr > threshold) && RS2_STATUS_ATOMIC == CLEAN) {
                        DPRINTF(Churn,
                                "DDAS CHURN STATUS WARN:: Storing CLEAN pointer to STALE mem location... how?\n");
                    }
                }*/
                //END PRIV:: Note, this code is to count threshold violations during churn (store(STALE->CLEAN))
                //		We don't need this and its causing errors, so removing...
            }

            DPRINTF(emtd, "0x%x: Wrote tag %s to memory 0x%x\n", pc, EMTD_TAG_NAMES[RS2_TAG_ATOMIC], get_mem_addr_atomic(inst, traceData));
        }

        /*** REG Arithmetic: Check tags of RS1 and RS2 for RD tag
                 Invalid Ops: Check the header file. This depends on the source tags
            ***/
        else if (Ops.is_reg_arith_op(opc))
        {
            // Outlawing any arithmetic on code pointers (this is actually C-standard)
            if (RS1_TAG_ATOMIC == CODE_PTR || RS2_TAG_ATOMIC == CODE_PTR)
            {
                sprintf(warn1, "0x%lu\n", pc);
                sprintf(warn2, "EMTD CHECK: Policy Violated on REG ARITH. Operand is CODE_PTR(3) %s.\n", opc.c_str());
                //DPRINTF(emtd_warning, "RS1 Tag: %s \tRS2 Tag: %s\n", EMTD_TAG_NAMES[RS1_TAG], EMTD_TAG_NAMES[RS2_TAG]);
                WRITE_RD_TAG_ATOMIC(DATA);
                WRITE_RD_STATUS_ATOMIC(CLEAN);
                //throw EMTD_INVALIDOP;
                record_violation(pc, std::string(warn2), std::string(warn1));
            }
            // All other ops should generate a DATA tag
            else
            {
                WRITE_RD_TAG_ATOMIC(DATA);
                WRITE_RD_STATUS_ATOMIC(CLEAN);
                DPRINTF(emtd, "0x%x: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], inst->destRegIdx(0).index());
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
            WRITE_RD_TAG_ATOMIC(RS1_TAG_ATOMIC);
            WRITE_RD_STATUS_ATOMIC(RS1_STATUS_ATOMIC);

            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[RS1_TAG_ATOMIC], inst->destRegIdx(0).index());
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
            WRITE_RD_TAG_ATOMIC(DATA);
            WRITE_RD_STATUS_ATOMIC(CLEAN);
            DPRINTF(emtd, "0x%lu: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], inst->destRegIdx(0).index());
        }

        /*** Jumps: Move tag of NPC into RD (saves a return addr)
                 Invalid Op: Address being used (RS1 for JALR only...) is not a CODE_PTR
            ***/
        else if (Ops.is_jump_op(opc))
        {
            if (opc == "jalr" && RS1_TAG_ATOMIC != CODE_PTR)
            {
                sprintf(warn1, "0x%lu\n", pc);
                sprintf(warn2, "EMTD CHECK: Policy Violated on JALR. Tag should be CODE_PTR(3), got %s.\n", EMTD_TAG_NAMES[RS1_TAG_ATOMIC].c_str());
                //throw EMTD_INVALIDOP;
                record_violation(pc, std::string(warn2), std::string(warn1));
            }

            // STACK FRAME HANDLING
            // Function Call? Save SP
            // CALLS == JAL/JALR's with RD == $ra
            if (inst->destRegIdx(0).index() == 1)
            {
                //assert(d_inst->traceData);
                //save_sp(d_inst->traceData->getThread());
                save_sp(tc);
            }
            // Function Return? Deallocate
            else if (opc == "jalr" && inst->srcRegIdx(0).index() == 1 && inst->destRegIdx(0).index() == 0)
            {
                deallocate_stack_tags();
            }
            // END STACK FRAME HANDLING

            //Write tag for next instruction if destination is not zero register
            if (inst->destRegIdx(0).index() != 0)
                WRITE_RD_TAG_ATOMIC(CODE_PTR);
            WRITE_RD_STATUS_ATOMIC(CLEAN);
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
            WRITE_RD_TAG_ATOMIC(DATA);
            WRITE_RD_STATUS_ATOMIC(CLEAN);
            DPRINTF(emtd, "0x%x: Wrote tag %s to register %x\n", pc, EMTD_TAG_NAMES[DATA], inst->destRegIdx(0).index());
        }
    }
    catch (int e)
    {
        exit(e);
    }
}

memaddr_t Metadata::get_mem_addr_atomic(StaticInstPtr inst, Trace::InstRecord *traceData)
{
    //Trace::InstRecord *iR = inst->traceData;
    //Addr addr = iR->getAddr();
    assert(traceData);
    return traceData->getAddr();
}

void Metadata::deallocate_stack_tags_atomic()
{
    // You are returning from a function and you need to dealloc
    if (base_sp.back() > max_sp.back())
    {
        clear_mem_range_tags(max_sp.back(), base_sp.back() - 8);
        DPRINTF(emtd, "Cleared stack from 0x%x to 0x%x\n", max_sp.back(), base_sp.back() - 8);
    }
    max_sp.pop_back();
    base_sp.pop_back();
}

// check and clear tags for deallocated stack
void Metadata::check_stack_pointer(ThreadContext *tc)
{
    if (!max_sp.empty() && max_sp.back() > tc->readIntReg(2))
    {
        max_sp.back() = tc->readIntReg(2);
    }
}

void Metadata::save_sp(ThreadContext *tc)
{
    base_sp.push_back(tc->readIntReg(2));
    max_sp.push_back(tc->readIntReg(2));
}

void Metadata::deallocate_stack_tags()
{
    // You are returning from a function and you need to dealloc
    if (base_sp.back() > max_sp.back())
    {
        clear_mem_range_tags(max_sp.back(), base_sp.back() - 8);
        DPRINTF(emtd, "Cleared stack from 0x%x to 0x%x\n", max_sp.back(), base_sp.back() - 8);
    }
    max_sp.pop_back();
    base_sp.pop_back();
}



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

Metadata *
MetadataParams::create()
{
    return new Metadata(this);
}
