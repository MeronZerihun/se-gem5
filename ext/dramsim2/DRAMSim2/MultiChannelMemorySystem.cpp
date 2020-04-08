//TODO: willAccptTransaction much check size of waitBuffer
//TODO: properly track and handle writes
//TODO: do not cache read requests from churn unit

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
#include <errno.h> 
#include <sstream> //stringstream
#include <stdlib.h> // getenv()
// for directory operations 
#include <sys/stat.h>
#include <sys/types.h>

#include "MultiChannelMemorySystem.h"
#include "AddressMapping.h"
#include "IniReader.h"


using namespace DRAMSim; 


MultiChannelMemorySystem::MultiChannelMemorySystem(const string &deviceIniFilename_, const string &systemIniFilename_, const string &pwd_, const string &traceFilename_, unsigned megsOfMemory_, string *visFilename_, const IniReader::OverrideMap *paramOverrides)
	:megsOfMemory(megsOfMemory_), deviceIniFilename(deviceIniFilename_),
	systemIniFilename(systemIniFilename_), traceFilename(traceFilename_),
	pwd(pwd_), visFilename(visFilename_), 
	clockDomainCrosser(new ClockDomain::Callback<MultiChannelMemorySystem, void>(this, &MultiChannelMemorySystem::actual_update)),
	csvOut(new CSVWriter(visDataOut))
{

	MEE_DEBUG("initializing");

	currentClockCycle=0; 
	if (visFilename)
		printf("CC VISFILENAME=%s\n",visFilename->c_str());

	if (!isPowerOfTwo(megsOfMemory))
	{
		ERROR("Please specify a power of 2 memory size"); 
		abort(); 
	}

	if (pwd.length() > 0)
	{
		//ignore the pwd argument if the argument is an absolute path
		if (deviceIniFilename[0] != '/')
		{
			deviceIniFilename = pwd + "/" + deviceIniFilename;
		}

		if (systemIniFilename[0] != '/')
		{
			systemIniFilename = pwd + "/" + systemIniFilename;
		}
	}

	DEBUG("== Loading device model file '"<<deviceIniFilename<<"' == ");
	IniReader::ReadIniFile(deviceIniFilename, false);
	DEBUG("== Loading system model file '"<<systemIniFilename<<"' == ");
	IniReader::ReadIniFile(systemIniFilename, true);

	// If we have any overrides, set them now before creating all of the memory objects
	if (paramOverrides)
		IniReader::OverrideKeys(paramOverrides);

	IniReader::InitEnumsFromStrings();
	if (!IniReader::CheckIfAllSet())
	{
		exit(-1);
	}

	if (NUM_CHANS == 0) 
	{
		ERROR("Zero channels"); 
		abort(); 
	}
	for (size_t i=0; i<NUM_CHANS; i++)
	{
		MemorySystem *channel = new MemorySystem(i, megsOfMemory/NUM_CHANS, (*csvOut), dramsim_log);
		channels.push_back(channel);
	}

	initTaggedSystem();
}
/* Initialize the ClockDomainCrosser to use the CPU speed 
	If cpuClkFreqHz == 0, then assume a 1:1 ratio (like for TraceBasedSim)
	*/
void MultiChannelMemorySystem::setCPUClockSpeed(uint64_t cpuClkFreqHz)
{

	uint64_t dramsimClkFreqHz = (uint64_t)(1.0/(tCK*1e-9));
	clockDomainCrosser.clock1 = dramsimClkFreqHz; 
	clockDomainCrosser.clock2 = (cpuClkFreqHz == 0) ? dramsimClkFreqHz : cpuClkFreqHz; 
}

bool fileExists(string &path)
{
	struct stat stat_buf;
	if (stat(path.c_str(), &stat_buf) != 0) 
	{
		if (errno == ENOENT)
		{
			return false; 
		}
		ERROR("Warning: some other kind of error happened with stat(), should probably check that"); 
	}
	return true;
}

string FilenameWithNumberSuffix(const string &filename, const string &extension, unsigned maxNumber=100)
{
	string currentFilename = filename+extension;
	if (!fileExists(currentFilename))
	{
		return currentFilename;
	}

	// otherwise, add the suffixes and test them out until we find one that works
	stringstream tmpNum; 
	tmpNum<<"."<<1; 
	for (unsigned i=1; i<maxNumber; i++)
	{
		currentFilename = filename+tmpNum.str()+extension;
		if (fileExists(currentFilename))
		{
			currentFilename = filename; 
			tmpNum.seekp(0);
			tmpNum << "." << i;
		}
		else 
		{
			return currentFilename;
		}
	}
	// if we can't find one, just give up and return whatever is the current filename
	ERROR("Warning: Couldn't find a suitable suffix for "<<filename); 
	return currentFilename; 
}
/**
 * This function creates up to 3 output files: 
 * 	- The .log file if LOG_OUTPUT is set
 * 	- the .vis file where csv data for each epoch will go
 * 	- the .tmp file if verification output is enabled
 * The results directory is setup to be in PWD/TRACEFILENAME.[SIM_DESC]/DRAM_PARTNAME/PARAMS.vis
 * The environment variable SIM_DESC is also appended to output files/directories
 *
 * TODO: verification info needs to be generated per channel so it has to be
 * moved back to MemorySystem
 **/
void MultiChannelMemorySystem::InitOutputFiles(string traceFilename)
{
	size_t lastSlash;
	size_t deviceIniFilenameLength = deviceIniFilename.length();
	string sim_description_str;
	string deviceName;
	
	char *sim_description = getenv("SIM_DESC");
	if (sim_description)
	{
			sim_description_str = string(sim_description);
	}


	// create a properly named verification output file if need be and open it
	// as the stream 'cmd_verify_out'
	if (VERIFICATION_OUTPUT)
	{
		string basefilename = deviceIniFilename.substr(deviceIniFilename.find_last_of("/")+1);
		string verify_filename =  "sim_out_"+basefilename;
		if (sim_description != NULL)
		{
			verify_filename += "."+sim_description_str;
		}
		verify_filename += ".tmp";
		cmd_verify_out.open(verify_filename.c_str());
		if (!cmd_verify_out)
		{
			ERROR("Cannot open "<< verify_filename);
			abort(); 
		}
	}
	// This sets up the vis file output along with the creating the result
	// directory structure if it doesn't exist
	if (VIS_FILE_OUTPUT)
	{
		stringstream out,tmpNum;
		string path;
		string filename;

		if (!visFilename)
		{
			path = "results/";
			// chop off the .ini if it's there
			if (deviceIniFilename.substr(deviceIniFilenameLength-4) == ".ini")
			{
				deviceName = deviceIniFilename.substr(0,deviceIniFilenameLength-4);
				deviceIniFilenameLength -= 4;
			}

			// chop off everything past the last / (i.e. leave filename only)
			if ((lastSlash = deviceName.find_last_of("/")) != string::npos)
			{
				deviceName = deviceName.substr(lastSlash+1,deviceIniFilenameLength-lastSlash-1);
			}

		string rest;
			// working backwards, chop off the next piece of the directory
			if ((lastSlash = traceFilename.find_last_of("/")) != string::npos)
			{
				traceFilename = traceFilename.substr(lastSlash+1,traceFilename.length()-lastSlash-1);
			}
			if (sim_description != NULL)
			{
				traceFilename += "."+sim_description_str;
			}

			if (pwd.length() > 0)
			{
				path = pwd + "/" + path;
			}

			// create the directories if they don't exist 
			mkdirIfNotExist(path);
			path = path + traceFilename + "/";
			mkdirIfNotExist(path);
			path = path + deviceName + "/";
			mkdirIfNotExist(path);

			// finally, figure out the filename
			string sched = "BtR";
			string queue = "pRank";
			if (schedulingPolicy == RankThenBankRoundRobin)
			{
				sched = "RtB";
			}
			if (queuingStructure == PerRankPerBank)
			{
				queue = "pRankpBank";
			}

			/* I really don't see how "the C++ way" is better than snprintf()  */
			out << (TOTAL_STORAGE>>10) << "GB." << NUM_CHANS << "Ch." << NUM_RANKS <<"R." <<ADDRESS_MAPPING_SCHEME<<"."<<ROW_BUFFER_POLICY<<"."<< TRANS_QUEUE_DEPTH<<"TQ."<<CMD_QUEUE_DEPTH<<"CQ."<<sched<<"."<<queue;
		}
		else //visFilename given
		{
			out << *visFilename;
		}
		if (sim_description)
		{
			out << "." << sim_description;
		}

		//filename so far, without extension, see if it exists already
		filename = out.str();


		filename = FilenameWithNumberSuffix(filename, ".vis"); 
		path.append(filename);
		cerr << "writing vis file to " <<path<<endl;


		visDataOut.open(path.c_str());
		if (!visDataOut)
		{
			ERROR("Cannot open '"<<path<<"'");
			exit(-1);
		}
		//write out the ini config values for the visualizer tool
		IniReader::WriteValuesOut(visDataOut);

	}
	else
	{
		// cerr << "vis file output disabled\n";
	}
#ifdef LOG_OUTPUT
	string dramsimLogFilename("dramsim");
	if (sim_description != NULL)
	{
		dramsimLogFilename += "."+sim_description_str; 
	}
	
	dramsimLogFilename = FilenameWithNumberSuffix(dramsimLogFilename, ".log"); 

	dramsim_log.open(dramsimLogFilename.c_str(), ios_base::out | ios_base::trunc );

	if (!dramsim_log) 
	{
	ERROR("Cannot open "<< dramsimLogFilename);
	//	exit(-1); 
	}
#endif

}


void MultiChannelMemorySystem::mkdirIfNotExist(string path)
{
	struct stat stat_buf;
	// check if the directory exists
	if (stat(path.c_str(), &stat_buf) != 0) // nonzero return value on error, check errno
	{
		if (errno == ENOENT) 
		{
//			DEBUG("\t directory doesn't exist, trying to create ...");

			// set permissions dwxr-xr-x on the results directories
			mode_t mode = (S_IXOTH | S_IXGRP | S_IXUSR | S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR) ;
			if (mkdir(path.c_str(), mode) != 0)
			{
				perror("Error Has occurred while trying to make directory: ");
				cerr << path << endl;
				abort();
			}
		}
		else
		{
			perror("Something else when wrong: "); 
			abort();
		}
	}
	else // directory already exists
	{
		if (!S_ISDIR(stat_buf.st_mode))
		{
			ERROR(path << "is not a directory");
			abort();
		}
	}
}


MultiChannelMemorySystem::~MultiChannelMemorySystem()
{
	for (size_t i=0; i<NUM_CHANS; i++)
	{
		delete channels[i];
	}
	channels.clear(); 

// flush our streams and close them up
#ifdef LOG_OUTPUT
	dramsim_log.flush();
	dramsim_log.close();
#endif
	if (VIS_FILE_OUTPUT) 
	{	
		visDataOut.flush();
		visDataOut.close();
	}
}
void MultiChannelMemorySystem::update()
{
	clockDomainCrosser.update(); 
}
void MultiChannelMemorySystem::actual_update() 
{
	if (currentClockCycle == 0)
	{
		InitOutputFiles(traceFilename);
		DEBUG("DRAMSim2 Clock Frequency ="<<clockDomainCrosser.clock1<<"Hz, CPU Clock Frequency="<<clockDomainCrosser.clock2<<"Hz"); 
	}

	if (currentClockCycle % EPOCH_LENGTH == 0)
	{
		(*csvOut) << "ms" <<currentClockCycle * tCK * 1E-6; 
		for (size_t i=0; i<NUM_CHANS; i++)
		{
			channels[i]->printStats(false); 
		}
		csvOut->finalize();
	}
	
	for (size_t i=0; i<NUM_CHANS; i++)
	{
		channels[i]->update(); 
	}
	#ifdef DETAILED_TAG
	tickTagSystem();

	#else 
	tickSimpleTagSystem();

	#endif


		currentClockCycle++; 
		current_cycle = currentClockCycle;
}
unsigned MultiChannelMemorySystem::findChannelNumber(uint64_t addr)
{
	// Single channel case is a trivial shortcut case 
	if (NUM_CHANS == 1)
	{
		return 0; 
	}

	if (!isPowerOfTwo(NUM_CHANS))
	{
		ERROR("We can only support power of two # of channels.\n" <<
				"I don't know what Intel was thinking, but trying to address map half a bit is a neat trick that we're not sure how to do"); 
		abort(); 
	}

	// only chan is used from this set 
	unsigned channelNumber,rank,bank,row,col;
	addressMapping(addr, channelNumber, rank, bank, row, col); 
	if (channelNumber >= NUM_CHANS)
	{
		ERROR("Got channel index "<<channelNumber<<" but only "<<NUM_CHANS<<" exist"); 
		abort();
	}
	//DEBUG("Channel idx = "<<channelNumber<<" totalbits="<<totalBits<<" channelbits="<<channelBits); 

	return channelNumber;

}
ostream &MultiChannelMemorySystem::getLogFile()
{
	return dramsim_log; 
}
bool MultiChannelMemorySystem::addTransaction(const Transaction &trans)
{
	assert(false && "Interface not supported for tagged mem");

	// copy the transaction and send the pointer to the new transaction 
	return addTransaction(new Transaction(trans)); 
}

bool MultiChannelMemorySystem::addTransaction(Transaction *trans)
{
	assert(false && "Interface not supported for tagged mem");

	unsigned channelNumber = findChannelNumber(trans->address); 
	return channels[channelNumber]->addTransaction(trans); 
}

bool MultiChannelMemorySystem::addTransaction(bool isWrite, uint64_t addr)
{
	#ifdef DETAILED_TAG	
	
	//for our tagged memory impl., we do not directly forward requests to a channel
	return addTaggedTransaction(isWrite, false, addr);
	
	#else

		unsigned channelNumber = findChannelNumber(addr); 
		return channels[channelNumber]->addTransaction(isWrite, addr);


	#endif

}

/*
	This function has two flavors: one with and without the address. 
	If the simulator won't give us an address and we have multiple channels, 
	we have to assume the worst and return false if any channel won't accept. 

	However, if the address is given, we can just map the channel and check just
	that memory controller
*/

bool MultiChannelMemorySystem::willAcceptTransaction(uint64_t addr)
{
	unsigned chan, rank,bank,row,col; 
	addressMapping(addr, chan, rank, bank, row, col); 
	#ifdef DETAILED_TAG
	return channels[chan]->WillAcceptTransaction() && tagCache->can_accept_input();
	
	#else
	return channels[chan]->WillAcceptTransaction();

	#endif
}

bool MultiChannelMemorySystem::willAcceptTransaction()
{
	for (size_t c=0; c<NUM_CHANS; c++) {
		if (!channels[c]->WillAcceptTransaction())
		{
			return false; 
		}
	}
	#ifdef DETAILED_TAG
	return tagCache->can_accept_input(); 
	#else
		return true;
	#endif
}



void MultiChannelMemorySystem::printStats(bool finalStats) {

	(*csvOut) << "ms" <<currentClockCycle * tCK * 1E-6; 
	for (size_t i=0; i<NUM_CHANS; i++)
	{
		PRINT("==== Channel ["<<i<<"] ====");
		channels[i]->printStats(finalStats); 
		PRINT("//// Channel ["<<i<<"] ////");
	}
	csvOut->finalize();
}
void MultiChannelMemorySystem::RegisterCallbacks( 
		TransactionCompleteCB *readDone,
		TransactionCompleteCB *writeDone,
		void (*reportPower)(double bgpower, double burstpower, double refreshpower, double actprepower))
{

	this->readDone = readDone;
	this->writeDone = writeDone;

	for (size_t i=0; i<NUM_CHANS; i++)
	{
		channels[i]->RegisterCallbacks(taggedReadCallback, taggedWriteCallback, reportPower); 
	}
}

/*
 * The getters below are useful to external simulators interfacing with DRAMSim
 *
 * Return value: 0 on success, -1 on error
 */
int MultiChannelMemorySystem::getIniBool(const std::string& field, bool *val)
{
	if (!IniReader::CheckIfAllSet())
		exit(-1);
	return IniReader::getBool(field, val);
}

int MultiChannelMemorySystem::getIniUint(const std::string& field, unsigned int *val)
{
	if (!IniReader::CheckIfAllSet())
		exit(-1);
	return IniReader::getUint(field, val);
}

int MultiChannelMemorySystem::getIniUint64(const std::string& field, uint64_t *val)
{
	if (!IniReader::CheckIfAllSet())
		exit(-1);
	return IniReader::getUint64(field, val);
}

int MultiChannelMemorySystem::getIniFloat(const std::string& field, float *val)
{
	if (!IniReader::CheckIfAllSet())
		exit(-1);
	return IniReader::getFloat(field, val);
}

namespace DRAMSim {
MultiChannelMemorySystem *getMemorySystemInstance(const string &dev, const string &sys, const string &pwd, const string &trc, unsigned megsOfMemory, string *visfilename) 
{
	return new MultiChannelMemorySystem(dev, sys, pwd, trc, megsOfMemory, visfilename);
}
}


//*************************************************************
//Tagged Memory Specific Code


void MultiChannelMemorySystem::initTaggedSystem(){

	MEE_DEBUG("initializing_tagged");

	callbackObj = new TaggedCallBack(&readResponse, &writeResponse);

	#ifdef DETAILED_TAG
	//initialize tag cache
	tagCache = new TagCache(this, TAG_CACHE_SIZE, TAG_CACHE_LATENCY);
	#endif

	taggedReadCallback = new Callback<TaggedCallBack, void, unsigned,
							 uint64_t, uint64_t>(callbackObj, &TaggedCallBack::readCallback);

	taggedWriteCallback = new Callback<TaggedCallBack, void, unsigned, 
							uint64_t, uint64_t>(callbackObj, &TaggedCallBack::writeCallback);


}


void  MultiChannelMemorySystem::tickSimpleTagSystem(){
	
	//on a tag cache miss, we will respond after a certain delay
	static queue<uint64_t> delayed_address;
	static queue<uint64_t> delayed_cycle;
	
	if(!writeResponse.empty()){
		
		(*writeDone)(0, writeResponse.front(), currentClockCycle);
		writeResponse.pop();
		
	}

	if(!readResponse.empty()){

		uint64_t address = readResponse.front();
		//if we hit on the tag cache, respond immediately
		//else delay by 50 cycles

		//note: SimpleTagCache will update LRU and contents when access is called
		unsigned delay = simpleTagCache.access(address % 2048, currentClockCycle);
		bool hit = delay == 0;
		if(hit){
			//return data back to CPU/TODO: track actual channel
			(*readDone)(0, address, currentClockCycle);	
		}
		else{
			MEE_DEBUG("tag miss 0x" << hex << address << dec);
			//add it to delay queue
			delayed_address.push(address);
			delayed_cycle.push(delay);
		}
		
		readResponse.pop();
		
	}

	//see if there is a ready delyed reponse.
	if(!delayed_cycle.empty() && delayed_cycle.front() <= currentClockCycle){
		(*readDone)(0, delayed_address.front(), currentClockCycle);	
		delayed_cycle.pop();
		delayed_address.pop();
	}
		
	

}


//clock tick
//TODO: break into multiple functions
void  MultiChannelMemorySystem::tickTagSystem(){

	if(!writeResponse.empty()){

		uint64_t resp_addr = writeResponse.front();
		bool is_cpu_ack=false;
		//check if this is a write ack for the CPU or tag cache
		for (auto it = cpuWriteRequests.begin(); it != cpuWriteRequests.end(); it++) {
			if(*it == resp_addr){
				(*writeDone)(0, resp_addr, currentClockCycle);
				cpuWriteRequests.erase(it);
				is_cpu_ack=true;
				break;
			}			
		}

		if(!is_cpu_ack){
			tagCache->write_done_callback();
		}

		writeResponse.pop();
		
	}

	if(!readResponse.empty()){

		uint64_t resp_addr = readResponse.front();

		MEE_DEBUG("response\t0x" << hex << resp_addr );

		//if it is a tag, return it to the tag cache
		//even if it is a reaquest from the churn unit, it goes through the tag cache.
		if( resp_addr >= TAG_ADDR_START ){
			MEE_DEBUG("DRAM_tag_response\t0x" << hex << resp_addr );
			tagResponses.push(resp_addr);
		}

		//otherwise, write it to the wait buffer where the tag and data are merged
		else{
			MEE_DEBUG("DRAM_data_response\t0x" << hex << resp_addr );
			for(TaggedData* t : waitBuffer ){
				if (t->data_addr == resp_addr){
					t->data_ready = true;
					break;
				}

			}
			

		}

		readResponse.pop();

	}


	//check if the tag cache has returned something
	if(tagCache->is_output_ready()){
		uint64_t tag_addr = tagCache->get_output();

		bool is_cpu_req = false;
		//check if this is an external tag read request (from churn)
		for (auto it = cpuReadRequests.begin(); it != cpuReadRequests.end(); it++) {
			if(*it == tag_addr){
				(*readDone)(0, tag_addr, currentClockCycle);
				cpuReadRequests.erase(it);
				is_cpu_req = true;
				break;
			}
			
		}
		
		//otherwise, send it to the wait buffer
		if(!is_cpu_req){

			for(TaggedData* t : waitBuffer ){
				if (t->tag_addr == tag_addr){
					t->tag_ready = true;
					break;
				}
			}

		}

	}


	

	//check if there is a ready request in the wait buffer.
	for (auto it = waitBuffer.begin(); it != waitBuffer.end(); it++) {
		if ((*it)->data_ready == true && (*it)->tag_ready == true){
			
			//return data back to CPU/TODO: track actual channel
			(*readDone)(0, (*it)->data_addr, currentClockCycle);

			//remove serviced request from wait buffer
			delete *it;
			waitBuffer.erase(it);
			break;
		}

	}



	tagCache->tick();

	//the tag cache must have read the response by now.
	if( !tagResponses.empty() ) tagResponses.pop();

}


uint64_t MultiChannelMemorySystem::getTagAddr(uint64_t addr){

	
	return TAG_ADDR_START + addr/32; //TODO: define division factor properly

}


bool MultiChannelMemorySystem::isTagRespAvailable(){
	return !tagResponses.empty();
}

uint64_t MultiChannelMemorySystem::getTagResponse(){
	return tagResponses.front();
}



bool MultiChannelMemorySystem::addTaggedTransaction(bool isWrite, bool isFromTagCache, uint64_t addr){

	MEE_DEBUG("processing_tagged_transaction\t0x" << hex << addr);

	unsigned channelNumber = findChannelNumber(addr); 
	
	//check DRAM channels can accept transaction
	bool accept = channels[channelNumber]->WillAcceptTransaction();

	//make sure if tag cache can also take input.
	//(if the request is from tag unit itself, we will not be sending requests to it...)
	if(!isFromTagCache){
		accept &= tagCache->can_accept_input();


	}

	if(!accept) return false;

	MEE_DEBUG("accepted");


	//tag requests from the churn unit are a special case - they are requests to the tag store that originate from
	//an external unit. 
	bool external_tag_read = !isWrite && !isFromTagCache && (addr > TAG_ADDR_START);
	bool external_tag_write = isWrite && !isFromTagCache && (addr > TAG_ADDR_START);
	bool external_tag_req = external_tag_read || external_tag_write;
	
	if(!isFromTagCache){

		MEE_DEBUG("processing_data_req");

		//for external requests (not from tag cache), we need to generate an additional request to 
		//the tag cache and wait for it.

		//if the churn unit is trying to read a tag, then we do not need to generate a new address
		//otherwise, for data requests, we need to calculate the address for the CPU
		uint64_t tagAddr = external_tag_req ? addr : getTagAddr(addr);  
		tagCache->add_input(isWrite, tagAddr);


		//for normal data reads, allocate space in wait buffer 
		//requests stay in the wait buffer until both data and tag are read
		if(!isWrite && !external_tag_read){
			TaggedData* td = new TaggedData(addr, getTagAddr(addr), false, false);
			waitBuffer.push_back(td);
		}


		//we need to remember these requests came from the CPU (not tag cache)
		//so that we can route requests back.
		if(isWrite)  cpuWriteRequests.push_back(addr);
		else cpuReadRequests.push_back(addr);

		
	}




	if(!external_tag_read) {
		//do regular read/write requests
		assert( channels[channelNumber]->addTransaction(isWrite, addr) ); 
	}

	return true;
		

}


//**************************************************************
