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
				DPRINTF(csd, "DISP %d\n", microops[i]->getDisp());
				//relocate
				numMicroops+=1;
				StaticInstPtr* tempmicroops = new StaticInstPtr[numMicroops];
				for(int j=0;j<i+1;j++){
					tempmicroops[j]=microops[j];
					tempmicroops[j]->clearLastMicroop();
				}
				

				//LB:: I think this constructor comes from ldstop.isa line 226, 260
				//	 	or line 100 of microldstop.hh
				StaticInstPtr injected = new X86ISAInst::Ld(
						machInst, 						//ExtMachInst _machInst
						"INJ_MOV_R_M", 					//const char * instMnem, in later versions this is just set to "injectedBranch"
						(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0 |  (1ULL << StaticInst::IsDataPrefetch), //uint64_t setFlags
						env.scale, 						// uint8_t _scale
						InstRegIndex(env.index), 		//InstRegIndex _index
						InstRegIndex(env.base), 		//InstRegIndex _base
						microops[i]->getDisp() + 8,		// uint64_t _disp
						InstRegIndex(env.seg), 			//InstRegIndex _segment
						InstRegIndex(microops[i]->destRegIdx(0).index()), 	// InstRegIndex _data
						env.dataSize, 					//uint8_t _dataSize
						env.addressSize, 				//uint8_t _addressSize
						0); 							//Request::FlagsType _memFlags
				injected->setInjected();

				//Sanity check for print
				DPRINTF(csd, "Load Ins:: %s\n", microops[i]->generateDisassembly(0, NULL));
			    DPRINTF(csd, "Inj Ins:: %s\n", injected->generateDisassembly(0, NULL));	

				tempmicroops[i+1]=injected;
				for(int j=i+2;j<numMicroops;j++){
					tempmicroops[j]=microops[j-1];
				}
				i++; //Skip injected microop

				//LB :: Dont actually change microops for testing purposes..
				numMicroops-=1;
				// delete [] microops;
				// microops = tempmicroops;

			}
		}
		microops[(numMicroops-1)]->setLastMicroop();
		ctx_decoded=true;
	}
	return 0;

}




}


