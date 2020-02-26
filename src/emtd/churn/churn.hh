#ifndef __Churn_HH__
#define __Churn_HH__

#include "params/Churn.hh"
#include "sim/sim_object.hh"
#include "mem/mem_object.hh"
#include "mem/cache/cache.hh"
#include "base/flags.hh"
//#include "cpu/minor/pipeline.hh"
//#include "cpu/minor/cpu.hh"
#include "cpu/o3/cpu.hh"
#include "cpu/o3/deriv.hh"
//#include "emtd/emtd_metadata.hh"
#include "debug/Churn.hh"
#include "base/loader/object_file.hh"
#include "emtd/Metadata.hh"
#include <unordered_map>
#include <queue>
#include <list>

using namespace std;


const uint64_t TAG_ADDR_OFFSET = (1L << 30);

const unsigned MEM_QUEUE_SIZE = 16;

enum ScanStatus {REG_SCAN, STACK_SCAN, HEAP_SCAN, BSS_SCAN, DATA_SCAN, INST_SCAN, IDLE};


class Churn : public MemObject
{
private:
    void processEvent();
    EventWrapper<Churn, &Churn::processEvent> event;

    Tick latency;
    bool no_ff;
    int timesLeft;

    ScanStatus scanStatus;

public:
    Addr reqAddr;

private:
    Addr sectionEndAddr;

		bool pipelineStalled;

    
    //for for debugging..
    // Addr instStartAddr = 0x1000;
    // Addr instEndAddr = 0x2000;

    // Addr stackStartAddr =0x7fffffffffffffff - 1024;
    // Addr stackEndAddr = 0x7fffffffffffffff;

    // Addr bssStartAddr = 0x100B0;
    // Addr bssEndAddr = 0x100B0 + 1024;

    // Addr dataStartAddr = 0x100B0 + 1024;
    // Addr dataEndAddr = 0x100B0 + 2048;

    //Addr heapEndAddr = 0x100B0 + 2048+1024;

    Addr heapStartAddr;


    uint64_t data_ptr_count = 0;
    uint64_t code_ptr_count = 0;
    uint64_t churn_duration_cycles = 0;


    unsigned maxPipelineStallCycles = 50;
    unsigned stalledCycles = 0;

    unordered_map<Addr, Addr> outstanding_tags;
    unordered_map<Addr, Addr> outstanding_ptrs;
    unordered_map<Addr, Addr> outstanding_insts;

    queue<Addr> pointerReadQueue;
    list< std::pair<int, PacketPtr> > pointerUpdateQueue;
    queue<Addr> tagReadQueue;


    queue<Addr> instReadQueue;
    queue< PacketPtr > instUpdateQueue;


    Addr getPhyAddr(Addr virtAddr, bool instFetch);
    uint8_t getTag(Addr);
    void processTag(Addr);
    void genMemRequest();
    void fetchNextTag();
    void fetchNextInst();
    void setupNextScan();


    //***********************************************************************************
    class MemSidePort : public MasterPort
    {
      private:
        /// The object that owns this object (MemObject)
        MemObject *owner;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;


      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        MemSidePort(const std::string& name, MemObject *owner) :
            MasterPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

      protected:
        /**
         * Receive a timing response from the slave port.
         */
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the slave port if sendTimingReq was called on this
         * master port (causing recvTimingReq to be called on the slave
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer slave
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
    };

    //***********************************************************************************


    MemSidePort memPort;

    DerivO3CPU* cpu;

    Cache* cache;

    bool enable_inst_reenc;


    uint64_t churnSleepCycles;
    const uint64_t INITIAL_CHURN_DELAY = 1e6;
    //ZBA: enable/disable churn
    //bool enableChurn;    

     /**
     * Handle the respone from the memory side
     *
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponse(PacketPtr pkt);
    


public:

        void startup();

        bool blocked;
	
	bool enable_ff;

        Churn(ChurnParams *params);
        
	//PRIV
	/*BaseMasterPort& getMasterPort(const std::string& if_name,
                                        PortID idx = InvalidPortID) override;*/
	//END PRIV:: Note, threw an error so removing it... we don't need churn for PRIV
                
};

#endif // __Churn_HH__
