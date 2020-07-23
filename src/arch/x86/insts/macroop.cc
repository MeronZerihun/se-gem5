/*
 * macroop.cc
 *
 *  Created on: Feb 27, 2017
 *      Author: mkt
 */


#include "macroop.hh"
#include "arch/x86/generated/decoder.hh"
#include "arch/x86/isa_traits.hh"
#include <iostream>
#include "debug/csd.hh"

namespace X86ISA
{
MacroopBase::MacroopBase(const char *mnem, ExtMachInst _machInst,
		uint32_t _numMicroops, X86ISA::EmulEnv _env) :
            				X86StaticInst(mnem, _machInst, No_OpClass),
							numMicroops(_numMicroops), env(_env)
{
	assert(numMicroops);
	microops = new StaticInstPtr[numMicroops];
	flags[IsMacroop] = true;
	ctx_decoded=false;
}

MacroopBase::~MacroopBase()
{
	delete [] microops;
}
int
MacroopBase::cTXAlterMicroops()
{
	DPRINTF(csd, "MacroopBase::cTXAlterMicroops()\n");
	if(ctx_decoded==false){
		for(int i=0;i<numMicroops;i++){
			if(microops[i]->isLoad())
			{
				DPRINTF(csd, "FOUND LOAD MICROOP\n");
				DPRINTF(csd, "SRC O %d\n", microops[i]->srcRegIdx(0));
				DPRINTF(csd, "DEST O %d\n", microops[i]->destRegIdx(0));

				//relocate
				numMicroops+=1;
				/StaticInstPtr*tempmicroops = new StaticInstPtr[numMicroops];
				for(int j=0;j<i+1;j++){
					tempmicroops[j]=microops[j];
					tempmicroops[j]->clearLastMicroop();
				}
				

				//LB:: I think this constructor comes from ldstop.isa line 226, 260
				//	 	or line 100 of microldstop.hh
				// StaticInstPtr injected = new X86ISAInst::Ld(
				// 		machInst, 					//ExtMachInst _machInst
				// 		macrocodeBlock, 			//const char * instMnem, in later versions this is just set to "injectedBranch"
				// 		(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0 |  (1ULL << StaticInst::IsDataPrefetch), //uint64_t setFlags
				// 		env.scale, 					// uint8_t _scale
				// 		InstRegIndex(env.index), 	//InstRegIndex _index
				// 		InstRegIndex(env.base), 	//InstRegIndex _base
				// 		0x4a3000,  					// uint64_t _disp
				// 		InstRegIndex(env.seg), 		//InstRegIndex _segment
				// 		InstRegIndex(NUM_INTREGS+0), // InstRegIndex _data
				// 		4, 							//uint8_t _dataSize
				// 		8, 							//uint8_t _addressSize
				// 		0 | Request::PREFETCH ); 	//Request::FlagsType _memFlags


				//From a later version... 
				StaticInstPtr injected = new X86ISAInst::Ld(
						machInst,
						"injetedCTX", 
						(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0 ,
						1 , 
						InstRegIndex(NUM_INTREGS+0), //index
						InstRegIndex(NUM_INTREGS+1) , //base
						0,	//disp
						InstRegIndex(env.seg), //seg
						InstRegIndex(NUM_INTREGS+2), //data
						4, 
						8, 
						0 );
				//injected->SetSrcRegIdx(2,NUM_INTREGS+3); ERROR
				injected->setInjected();

				std::string dis = injected->generateDisassembly(0, NULL);
			        DPRINTF(csd, "Inj Ins:: %s\n", dis);	
				
				//Index, Base, Data
				// 0, 1, 2
				//Inj Ins::   injetedBranch : ld   t2d, DS:[t1]

				//injected->SetSrcRegIdx(2,NUM_INTREGS+3); //From a later version... 
				injected->setInjected();
				//injected->printFlags(std::cout," ");
				tempmicroops[i+1]=injected;
				for(int j=i+2;j<numMicroops;j++){
					tempmicroops[j]=microops[j-1];
				}
				i++;
				delete [] microops;
				microops = tempmicroops;

			}
		}
		microops[(numMicroops-1)]->setLastMicroop();
		ctx_decoded=true;
	}
	return 0;

}




}


