#ifndef MULT_MEM_SYS
#define MULT_MEM_SYS

/*********************************************************************************
*  Copyright (c) 2010-2011, Elliott Cooper-Balis
*                             Paul Rosenfeld
*                             Bruce Jacob
*                             University of Maryland 
*                             dramninjas [at] gmail [dot] com
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

namespace DRAMSim{
	class MultiChannelMemorySystem;
}


#include "SimulatorObject.h"
#include "Transaction.h"
#include "SystemConfiguration.h"
#include "MemorySystem.h"
#include "IniReader.h"
#include "ClockDomain.h"
#include "CSVWriter.h"
#include "tag_cache.h"
#include "simple_tag_cache.h"

#define TAG_ADDR_START (1L << 30)


namespace DRAMSim {


class MultiChannelMemorySystem : public SimulatorObject 
{
	public: 

	MultiChannelMemorySystem(const string &dev, const string &sys, const string &pwd, const string &trc, unsigned megsOfMemory, string *visFilename=NULL, const IniReader::OverrideMap *paramOverrides=NULL);
		virtual ~MultiChannelMemorySystem();
			bool addTransaction(Transaction *trans);
			bool addTransaction(const Transaction &trans);
			bool addTransaction(bool isWrite, uint64_t addr);
			bool willAcceptTransaction(); 
			bool willAcceptTransaction(uint64_t addr); 
			void update();
			void printStats(bool finalStats=false);
			ostream &getLogFile();
			void RegisterCallbacks( 
				TransactionCompleteCB *readDone,
				TransactionCompleteCB *writeDone,
				void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower));
			int getIniBool(const std::string &field, bool *val);
			int getIniUint(const std::string &field, unsigned int *val);
			int getIniUint64(const std::string &field, uint64_t *val);
			int getIniFloat(const std::string &field, float *val);

	void InitOutputFiles(string tracefilename);
	void setCPUClockSpeed(uint64_t cpuClkFreqHz);

	//output file
	std::ofstream visDataOut;
	ofstream dramsim_log; 

	vector<MemorySystem*> channels; 

	//===================tag system specific code ================================


	//used by the tag cache to send and recieve DRAM read/write requests
	bool isTagRespAvailable(); //the tag cache polls DRAM controller using this function
	uint64_t getTagResponse(); 
	

	bool addTaggedTransaction(bool isWrite, bool isFromTagCache, uint64_t addr);


		
	//when reads are done, we do not directly get back to the CPU simulator, 
	//instead, we do some processing in the controller itself.
	//if the response is not for a tag, we call the CPU simulator's callback
	TransactionCompleteCB *taggedReadCallback,*taggedWriteCallback; //callbacks in the controller
	TransactionCompleteCB *readDone, *writeDone; //the CPUs callback.





	class TaggedCallBack{
		public:

			TaggedCallBack(queue<uint64_t>* readQueue_, queue<uint64_t>* writeQueue_):readQueue(readQueue_), writeQueue(writeQueue_) {}
			

			void readCallback(unsigned id, uint64_t address, uint64_t clock_cycle){
				readQueue->push(address);
			}

			void writeCallback(unsigned id, uint64_t address, uint64_t clock_cycle){
				writeQueue->push(address);

			}		

		private:
			queue<uint64_t>* readQueue;
			queue<uint64_t>* writeQueue;

	};
	
	TaggedCallBack *callbackObj;



	//===============================================================================

	private:
		unsigned findChannelNumber(uint64_t addr);
		void actual_update(); 
		
		unsigned megsOfMemory; 
		string deviceIniFilename;
		string systemIniFilename;
		string traceFilename;
		string pwd;
		string *visFilename;
		ClockDomain::ClockDomainCrosser clockDomainCrosser; 
		static void mkdirIfNotExist(string path);
		static bool fileExists(string path); 
		CSVWriter *csvOut; 


		//===================tag system specific code ================================

		SimpleTagCache simpleTagCache;
		
		//TODO: remove old/detailed tagged implementation code
		TagCache* tagCache;

		queue<uint64_t>	readResponse;
		queue<uint64_t>	writeResponse;

		
		queue<uint64_t> tagResponses; //responses from DRAM that need
									 // to be forwarded to tag caches
		
		
		struct TaggedData{
			uint64_t data_addr;
			uint64_t tag_addr;
			bool data_ready;
			bool tag_ready;
			TaggedData(uint64_t data_addr, uint64_t tag_addr, bool data_ready, bool tag_ready){

				this->data_addr = data_addr;
				this->tag_addr = tag_addr;
				this->data_ready = data_ready;
				this->tag_ready = tag_ready;

			}
		};

		vector<TaggedData*> waitBuffer;
		vector<uint64_t> cpuWriteRequests;
		vector<uint64_t> cpuReadRequests;

		void initTaggedSystem();		

		
		void tickTagSystem();
		void tickSimpleTagSystem();
		uint64_t getTagAddr(uint64_t);

		uint64_t current_cycle;

		//===============================================================================

	};
}

#endif
