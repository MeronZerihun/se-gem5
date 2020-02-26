#include "emtd/churn/churn.hh"

#include <iostream>


//#define DEBUG_CHURN 1

const int BLOCK_SIZE = 64;
const int MASTER_ID = 4;





Churn::Churn(ChurnParams *params) :
    MemObject(params), event(*this),pipelineStalled(false),
    memPort(params->name + ".mem_side", this), cpu(params->cpu), cache(params->cache), enable_inst_reenc(params->enable_inst_reenc), churnSleepCycles(params->churnSleepCycles), blocked(false), enable_ff(params->enable_ff)
{
    //we need to capture the heap start address when program initializes.    
    Process *process = cpu->thread[0]->getTC()->getProcessPtr();
    heapStartAddr = process->memState->getBrkPoint();

}

//PRIV
/*
BaseMasterPort&
Churn::getMasterPort(const std::string& if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    // This is the name from the Python SimObject declaration (Churn.py)
    if (if_name == "mem_side") {
        return memPort;
    } else {
        // pass it along to our super class
        return MemObject::getMasterPort(if_name, idx);
    }
}*/
//END PRIV:: Note, threw an error so removing... we don't need churn for PRIV

void
Churn::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
        ((Churn*)owner)->blocked=true;
    }
}


uint8_t Churn::getTag(Addr virtAddr){
    
    #ifdef DEBUG_CHURN
        //for debugging we will assume a portion of tags are code "pointer"s
        
        if (virtAddr % 192 == 0 )  return CODE_PTR;
        else return DATA;
        

    #else 
       
       return cpu->commit.metadata->get_mem_tag(virtAddr);
       
    #endif

    

}

void 
Churn::processTag(Addr phyTagAddr){


    //DPRINTF(Churn, "processing tag\n");

    
    Addr virtAddr = outstanding_tags[phyTagAddr];
    

    //go through tags for all 8 words
    //the tag checks can be done in parallel.
    //but the memory requests have to be sent out serially

    bool send_update = false;

    for (int i = 0; i < 64; i+=8)
    {
        uint8_t tag = getTag(virtAddr + i);

        //if tag is for code pointer, fetch the associated code pointer so that it can be updated
        if(tag == CODE_PTR){
            send_update = true;
        }

        if(tag == CODE_PTR) code_ptr_count++;
    }

    //we send a single update that batches all the updates to a single cache line.
    Addr ptrAddr = phyTagAddr - TAG_ADDR_OFFSET;
    if(send_update){
        pointerReadQueue.push(ptrAddr);
        outstanding_ptrs[ptrAddr] = virtAddr; //we really have no use for the virtaddress in this case. only need to track outstanding requests
    }
    

}

bool
Churn::MemSidePort::recvTimingResp(PacketPtr pkt)
{

    //DPRINTF(Churn, "response ready \n");

    Addr phyAddr = pkt->getAddr();

    //check if this is a tag or a pointer fetch
    bool is_tag = ((Churn*)owner)->outstanding_tags.count(phyAddr) != 0;
    bool is_ptr_read = ((Churn*)owner)->outstanding_ptrs.count(phyAddr) != 0; 
    bool is_inst_read = ((Churn*)owner)->outstanding_insts.count(phyAddr) != 0; 



    if(is_tag){

        ((Churn*)owner)->processTag(phyAddr);
        ((Churn*)owner)->outstanding_tags.erase(phyAddr);

    }

    else if(is_inst_read){

        ((Churn*)owner)->instUpdateQueue.push(pkt);
        ((Churn*)owner)->outstanding_insts.erase(phyAddr);

    }
    
    else if(is_ptr_read) {


        

        //this is a pointer read. 
        //so simply generate another write to simulate contention due to updates
        //an actual implementation would generate a new memory block with the extracted pointer 
        //incremented by some offset.
        ((Churn*)owner)->pointerUpdateQueue.push_back(std::make_pair(20, pkt));
        ((Churn*)owner)->outstanding_ptrs.erase(phyAddr);


    }

    return true;
}


void
Churn::MemSidePort::recvRangeChange()
{ }



void
Churn::MemSidePort::recvReqRetry()
{
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;
    ((Churn*)owner)->blocked=false;

    sendPacket(pkt);
        

}


Addr Churn::getPhyAddr(Addr virtAddr, bool inst_fetch=false){

    Request tlb_request;    
    tlb_request.setContext( cpu->thread[0]->getTC()->contextId() );


     //setVirt(int asid, Addr vaddr, unsigned size, Flags flags, MasterID mid, Addr pc)


    Addr aligned_addr = virtAddr & ~0x3f; 

    if(inst_fetch){
        tlb_request.setVirt(0 /* asid */,
            aligned_addr, BLOCK_SIZE, Request::INST_FETCH, cpu->instMasterId(),
            /* I've no idea why we need the PC, but give it */
            virtAddr);

    }
    else{

        tlb_request.setVirt(0 /* asid */,
            aligned_addr, BLOCK_SIZE, Request::PRIVILEGED, cpu->instMasterId(),
            /* I've no idea why we need the PC, but give it */
            virtAddr);

    }


    assert(NoFault == cpu->itb->translateAtomic( &tlb_request, cpu->getContext(0), BaseTLB::Read  ) );

    return tlb_request.getPaddr();

}


//this is the module where the read and write requests are actually generated
void Churn::genMemRequest(){

    //the interconnect cannot take any new requests
    if(blocked) {
        //DPRINTF(Churn, "req blocked\n");
        return;  
    } 

    Packet::Command cmd;
    PacketPtr pkt;
    Addr phyAddr;
    Request::FlagsType flags = 0;

    PacketDataPtr data;

    //memory requests go out in the following priority: pointer update, pointer fetch, instruction fetch, instruction update, tag fetch
    //this will not result in indifinite starvation as new pointer fetch/update requests will not be generated
    //while tag fetches are stalled.

    if(!pointerUpdateQueue.empty() && pointerUpdateQueue.front().first<=0){
        //DPRINTF(Churn, "req from pointer update queue\n");
        PacketPtr reqPkt = pointerUpdateQueue.front().second;
        phyAddr = reqPkt->getAddr();
        pointerUpdateQueue.pop_front();
	delete(reqPkt);
        //cmd = MemCmd::WriteReq;
        //data = reqPkt->getPtr<uint8_t>();
        cmd = MemCmd::ReadReq;
        data = new uint8_t[BLOCK_SIZE];

        Request *req = new Request(phyAddr, BLOCK_SIZE, flags, MASTER_ID);
        pkt = new Packet(req, cmd, BLOCK_SIZE);
        pkt->dataStatic(data);       
        memPort.sendPacket(pkt);
      
    }

    else if(!pointerReadQueue.empty()) {
        //DPRINTF(Churn, "req from pointer read queue\n");
        
        //we want to get exclusive access since we intend to overwrite a value
        //this shall invalidate the core's caches
        cmd = MemCmd::ReadReq;
        phyAddr = pointerReadQueue.front();
        pointerReadQueue.pop();
        data = new uint8_t[BLOCK_SIZE];

        Request *req = new Request(phyAddr, BLOCK_SIZE, flags, MASTER_ID);
        pkt = new Packet(req, cmd, BLOCK_SIZE);
        pkt->dataStatic(data);
        memPort.sendPacket(pkt);
    }

    else if(!instUpdateQueue.empty()){
        //DPRINTF(Churn, "req from pointer update queue\n");
        PacketPtr reqPkt = instUpdateQueue.front();
        phyAddr = reqPkt->getAddr();
        instUpdateQueue.pop();
	delete(reqPkt);
        //cmd = MemCmd::WriteReq;
        //data = reqPkt->getPtr<uint8_t>();
        cmd = MemCmd::ReadReq;
        data = new uint8_t[BLOCK_SIZE];

        Request *req = new Request(phyAddr, BLOCK_SIZE, flags, MASTER_ID);
        pkt = new Packet(req, cmd, BLOCK_SIZE);
        pkt->dataStatic(data);
        memPort.sendPacket(pkt);        
    }


    else if(!instReadQueue.empty()) {
        //DPRINTF(Churn, "req from pointer read queue\n");
        
        //we want to get exclusive access since we intend to overwrite a value
        //this shall invalidate the core's caches
        cmd = MemCmd::ReadReq;
        phyAddr = instReadQueue.front();
        instReadQueue.pop();
        //construct the actual packet
        data = new uint8_t[BLOCK_SIZE];

        Request *req = new Request(phyAddr, BLOCK_SIZE, flags, MASTER_ID);
        pkt = new Packet(req, cmd, BLOCK_SIZE);
        pkt->dataStatic(data);
        memPort.sendPacket(pkt);
    }

    else if(!tagReadQueue.empty()){
        //DPRINTF(Churn, "req from tag read queue\n");

        cmd = MemCmd::ReadReq; 
        phyAddr = tagReadQueue.front();
        tagReadQueue.pop();
        //construct the actual packet
        data =  new uint8_t[BLOCK_SIZE];

        Request *req = new Request(phyAddr, BLOCK_SIZE, flags, MASTER_ID);
        pkt = new Packet(req, cmd, BLOCK_SIZE);
        pkt->dataStatic(data);
        memPort.sendPacket(pkt);
    }

    else{
        //DPRINTF(Churn, "no mem request\n");
        return;
    }

}



void Churn::fetchNextInst(){

    
    //for our current model, it does not make sense whether we are updating pointers or instructions.
    //TODO: add some additional latency to account for 3 or 4 extra cycles from qarma?

    Addr reqAddr = cpu->commit.metadata->get_threshold();
    if(reqAddr < sectionEndAddr && instReadQueue.size() < MEM_QUEUE_SIZE){

        Addr phyAddr = getPhyAddr(reqAddr, true); 
        instReadQueue.push(phyAddr);
        outstanding_insts[phyAddr] = reqAddr;
        cpu->commit.metadata->inc_threshold(BLOCK_SIZE);

    }

}

void Churn::fetchNextTag(){

    //TODO: constrain outstanding requests so that pointer read/write queues usage will be capped.


    if( scanStatus == IDLE ) return;

    //DPRINTF(Churn, "Queue size: %d\n", tagReadQueue.size());

    Addr reqAddr = cpu->commit.metadata->get_threshold();
    if(reqAddr < sectionEndAddr && tagReadQueue.size() < MEM_QUEUE_SIZE){


        //DPRINTF(Churn, "translating address %0x\n", reqAddr);
        Addr phyAddr = getPhyAddr(reqAddr) + TAG_ADDR_OFFSET; 
        
        //if some cases, data from two different sections will end up in the same memory block. 
        //in that case we want to avoid generating two duplicate requests to the same memory block
        if(outstanding_tags.count(phyAddr)==0){
            outstanding_tags[phyAddr] = reqAddr;
            tagReadQueue.push(phyAddr);
        }

        cpu->commit.metadata->inc_threshold(BLOCK_SIZE);

    }


}

void Churn::setupNextScan(){


    //Process *process = cpu->thread[0]->getTC()->getProcessPtr();
    Addr reqAddr = 0;

    switch(scanStatus){

        case INST_SCAN:
            scanStatus = IDLE;
            cpu->commit.metadata->end_churn();
            break;
        case IDLE:
            //schedule to re-launch churn...
            scanStatus = REG_SCAN;
            cpu->commit.metadata->begin_churn();
            break;

        /*case REG_SCAN:
            scanStatus = STACK_SCAN;
            reqAddr = process->memState->getStackBase() - process->memState->getStackSize();
            cpu->commit.metadata->set_threshold(reqAddr);
            sectionEndAddr = process->memState->getStackBase();
            DPRINTF(Churn, "Scheduling stack scan:");
            break;

        case STACK_SCAN:
            scanStatus = BSS_SCAN;
            reqAddr = process->objFile->bssBase();
            cpu->commit.metadata->set_threshold(reqAddr);
            sectionEndAddr = reqAddr + process->objFile->bssSize();
            DPRINTF(Churn, "Scheduling bss scan:");
            break;

        case BSS_SCAN:
            scanStatus = DATA_SCAN;
            reqAddr = process->objFile->dataBase();
            cpu->commit.metadata->set_threshold(reqAddr);
            sectionEndAddr = reqAddr + process->objFile->dataSize();
            DPRINTF(Churn, "Scheduling data scan:");
            break;


        case DATA_SCAN:
            scanStatus = HEAP_SCAN;
            reqAddr = heapStartAddr;
            cpu->commit.metadata->set_threshold(reqAddr);
            sectionEndAddr = process->memState->getBrkPoint();
            DPRINTF(Churn, "Scheduling heap scan:");
            break;

        case HEAP_SCAN:

            if(enable_inst_reenc){
                reqAddr = process->objFile->textBase();
                cpu->commit.metadata->set_threshold(reqAddr);
                sectionEndAddr = reqAddr + process->objFile->textSize();
                DPRINTF(Churn, "Scheduling inst scan:");
                scanStatus = INST_SCAN;
            }
            else{
                scanStatus = IDLE;   
            }
            break;*/
	default:
	    scanStatus = IDLE;
	    break;
    }


    if(scanStatus!=IDLE && scanStatus!=REG_SCAN){
        DPRINTF(Churn, " %d bytes\n", sectionEndAddr - reqAddr);
        //DPRINTF(Churn, "Scan Range: %x -> %x\n", reqAddr, sectionEndAddr);
    }
    

        
    
}


void
Churn::processEvent()
{
    //If we just switched to this CPU, schedule next event
    //if (!enable_ff || cpu->enable_churn){

	//Go through pointer update list and decrement each count
	for(auto it = pointerUpdateQueue.begin(); it != pointerUpdateQueue.end(); ++it){
		if(it->first != 0){
  			it->first = it->first - 1;
		}
	}


        //we need to kick-off churn by stalling the pipeline
        if (scanStatus == REG_SCAN && !pipelineStalled) {


            assert(tagReadQueue.empty() && pointerReadQueue.empty() && pointerUpdateQueue.empty());

            churn_duration_cycles++;

            DPRINTF(Churn, "Stalling Pipeline\n");
   	    cpu->drain();
   	    pipelineStalled = true;

            //we will not do anything while the pipeline is being updated
            //schedule(event, clockEdge( Cycles(tagUpdateStallCycles) )  );
            schedule(event, nextCycle());


            //TODO: start scan in parallel and save a few cycles?



        }

            //we are done updating, flushing, and refilling the pipeline
            //so resume execution
        else if (scanStatus == REG_SCAN && pipelineStalled) {

            churn_duration_cycles++;

            stalledCycles++;

            //if (cpu->pipeline->isDrained() || stalledCycles > maxPipelineStallCycles)
            if (cpu->isCpuDrained()) {

                DPRINTF(Churn, "Resuming pipeline after %d stalled cycles\n", stalledCycles);
                stalledCycles=0;
		pipelineStalled = false;
                cpu->drainResume();
                setupNextScan(); //start scanning memory in the next cycle
            }


	    // Stall for an additonal 30 cycles to simulate register updates
            schedule(event, clockEdge(Cycles(30)));

        } else if (scanStatus == INST_SCAN) {

            churn_duration_cycles++;

            fetchNextInst();

            //check if we are done fetching instructions
            if (cpu->commit.metadata->get_threshold() >= sectionEndAddr) {
                setupNextScan();
            }

            schedule(event, nextCycle());
        }

            //we are scanning tags
        else if (scanStatus != IDLE) {

            churn_duration_cycles++;


            //for stack and heap scans, memory could be deallocated
            //in the middle of churn. so, we update the scan range accordingly

            Process *process = cpu->thread[0]->getTC()->getProcessPtr();

            if (scanStatus == HEAP_SCAN) {
                sectionEndAddr = process->memState->getBrkPoint();
            } else if (scanStatus == STACK_SCAN) {
                //the stack grows down, so we need to move our current scan position further up
                //if we the memory being scanned gets churned.

                Addr stackTop = process->memState->getStackBase() - process->memState->getStackSize();

                if (cpu->commit.metadata->get_threshold() < stackTop) cpu->commit.metadata->set_threshold(stackTop);

            }



            //DPRINTF(Churn, "not idle %d -> %d\n", reqAddr, sectionEndAddr);

            fetchNextTag(); //we do tag scans in parallel with execution

            //if we are done scanning the current section, schedule another section.
            if (cpu->commit.metadata->get_threshold() >= sectionEndAddr) {
                setupNextScan();
            }

            schedule(event, nextCycle());


        } else if (scanStatus == IDLE) {

            bool done = !blocked && tagReadQueue.empty() && pointerReadQueue.empty() && pointerUpdateQueue.empty() &&
                        outstanding_tags.empty() && outstanding_ptrs.empty() &&
                        outstanding_insts.empty() && instUpdateQueue.empty() && instReadQueue.empty();

            if (done) {
                DPRINTF(Churn, "churn %d cycles!\n", churn_duration_cycles);
                DPRINTF(Churn, "churned code pointers: %d \t churned data pointers %d \n", code_ptr_count,
                        data_ptr_count);

                uint64_t sleep_cycles = 1;
                if (churnSleepCycles > churn_duration_cycles) {
                    sleep_cycles = churnSleepCycles - churn_duration_cycles;
                } else {
                    DPRINTF(Churn, "doing continuous churn\n");
                }

                code_ptr_count = 0;
                data_ptr_count = 0;
                churn_duration_cycles = 0;

                setupNextScan();
                schedule(event, clockEdge(Cycles(sleep_cycles)));
            } else {
                schedule(event, nextCycle());
            }

        } else {
            DPRINTF(Churn, "unknown state - what are you doing here!!?!?\n");
            assert(false && "unknown state");
        }


        //keep draining memory queues
        genMemRequest();

    //}
    //else {
    //    schedule(event, nextCycle());
    //}

    
    
}


void Churn::startup()
{

    //schedule to launch churn
    DPRINTF(Churn, "Churn startup\n");
    DPRINTF(Churn, "Churn Sleeping for %d cycles \n", churnSleepCycles);
    DPRINTF(Churn, "Instruction Re-Encryption: %d \n", enable_inst_reenc);

    scanStatus = REG_SCAN; 
    
    schedule(event, clockEdge( Cycles(INITIAL_CHURN_DELAY) )  );
}


Churn*
ChurnParams::create()
{
    return new Churn(this);
}
