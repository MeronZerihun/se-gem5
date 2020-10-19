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
#include "arch/x86/regs/float.hh"
#include <string>

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




/***************************************************************/
/*  	LOAD INJECTION										   */
/***************************************************************/
int 
MacroopBase::countLoadMicros (StaticInstPtr load_microop){

	X86ISA::InstRegIndex dest = InstRegIndex(load_microop->destRegIdx(0).index());
	X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(load_microop->srcRegIdx(1).index());
	}

	if (dest == ptr) {return 3;}

	return 2;
}

std::vector<StaticInstPtr>
MacroopBase::injectLoadMicros (StaticInstPtr load_microop){

	std::vector<StaticInstPtr> result;
	
	X86ISA::InstRegIndex dest = InstRegIndex(load_microop->destRegIdx(0).index());
	X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(load_microop->srcRegIdx(1).index());
	}
	std::string opName = load_microop->getName();


	if(dest == ptr){
	// MOV constructor originates from microregop.hh::85
		StaticInstPtr inj_mov = new X86ISAInst::Mov(
				machInst, 							//ExtMachInst _machInst
				"INJ_MOV", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				ptr, 								//InstRegIndex _src1
				ptr, 								//InstRegIndex _src2
				InstRegIndex(NUM_INTREGS+1), 		//InstRegIndex _dest
				env.dataSize, 						//uint8_t _dataSize
				0); 								//uint16_t _ext
		inj_mov->setInjected();
		inj_mov->clearLastMicroop();
		result.push_back(inj_mov);
		ptr = InstRegIndex(NUM_INTREGS+1); //New ptr is temp register
	}

	
	if(opName.compare("ld")==0){
		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_LD", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr existing_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"MOV_M_R",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_load->clearLastMicroop();
		result.push_back(existing_load);

		StaticInstPtr inj_dec = new X86ISAInst::Dec(
					machInst, 							//ExtMachInst _machInst
					"INJ_DEC", 							//const char * instMnem
					(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
					dest, 								//InstRegIndex _src1
					dest, 								//InstRegIndex _src2
					dest, 								//InstRegIndex _dest
					8, 									//uint8_t _dataSize
					0); 								//uint16_t _ext
		inj_dec->setInjected();
		inj_dec->clearLastMicroop();
		result.push_back(inj_dec);
	}
	else if(opName.compare("ldfp")==0){
		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr inj_load = new X86ISAInst::Ldfp(
				machInst, 							//ExtMachInst _machInst
				"INJ_LD", 							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		//LOAD constructor originates from microldstop.hh::100
		StaticInstPtr existing_load = new X86ISAInst::Ldfp(
				machInst, 							//ExtMachInst _machInst
				"MOV_M_R",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				load_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_load->clearLastMicroop();
		result.push_back(existing_load);

		// DEC constructor originates from microregop.hh::85
		std::__cxx11::string diss = load_microop->generateDisassembly(0, NULL);
		if(diss.find("_low") != std::string::npos){
			dest = InstRegIndex(FLOATREG_XMM_LOW(env.reg));
		}
		else if (diss.find("_high") != std::string::npos){
			dest = InstRegIndex(FLOATREG_XMM_HIGH(env.reg));
		}
		StaticInstPtr inj_dec = new X86ISAInst::DecFP(
					machInst, 							//ExtMachInst _machInst
					"INJ_DEC", 							//const char * instMnem
					(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
					dest, 								//InstRegIndex _src1
					dest, 								//InstRegIndex _src2
					dest, 								//InstRegIndex _dest
					8, 									//uint8_t _dataSize
					0); 								//uint16_t _ext
		inj_dec->setInjected();
		inj_dec->clearLastMicroop();
		result.push_back(inj_dec);
	}
	else{
		DPRINTF(csd, "WARNING:: OpName not handled by load injection in cTXAlterMicroops():: %s\n", opName);
	}



	return result;
}




/***************************************************************/
/*  	STORE INJECTION										   */
/***************************************************************/
int 
MacroopBase::countStoreMicros (StaticInstPtr store_microop){
	return 2;
}

StaticInstPtr MacroopBase::getInjInsn_Enc(InstRegIndex dest){
		// ENC constructor originates from microregop.hh::8
	DPRINTF(csd, "WARNING:: UNIMPLEMENTED %d\n", curTick());

	StaticInstPtr inj_enc; 
	int update_time= 0; 

	//Placeholder
	// if(0 >= metadata->get_enc_latency()){
	// 	inj_enc = new X86ISAInst::EncNone( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 8, 0); 
	// 	return inj_enc;
	// }

	switch(update_time){
		case 0: inj_enc = new X86ISAInst::Enc( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 1: inj_enc = new X86ISAInst::Enc1( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 2: inj_enc = new X86ISAInst::Enc2( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 3: inj_enc = new X86ISAInst::Enc3( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 4: inj_enc = new X86ISAInst::Enc4( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 5: inj_enc = new X86ISAInst::Enc5( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 6: inj_enc = new X86ISAInst::Enc6( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 7: inj_enc = new X86ISAInst::Enc7( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 8: inj_enc = new X86ISAInst::Enc8( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 9: inj_enc = new X86ISAInst::Enc9( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   
			break;
		case 10: inj_enc = new X86ISAInst::Enc10( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 11: inj_enc = new X86ISAInst::Enc11( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 12: inj_enc = new X86ISAInst::Enc12( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 13: inj_enc = new X86ISAInst::Enc13( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 14: inj_enc = new X86ISAInst::Enc14( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 15: inj_enc = new X86ISAInst::Enc15( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 16: inj_enc = new X86ISAInst::Enc16( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 17: inj_enc = new X86ISAInst::Enc17( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 18: inj_enc = new X86ISAInst::Enc18( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 19: inj_enc = new X86ISAInst::Enc19( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 20: inj_enc = new X86ISAInst::Enc20( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 21: inj_enc = new X86ISAInst::Enc21( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 22: inj_enc = new X86ISAInst::Enc22( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 23: inj_enc = new X86ISAInst::Enc23( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 24: inj_enc = new X86ISAInst::Enc24( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 25: inj_enc = new X86ISAInst::Enc25( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 26: inj_enc = new X86ISAInst::Enc26( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 27: inj_enc = new X86ISAInst::Enc27( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 28: inj_enc = new X86ISAInst::Enc28( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 29: inj_enc = new X86ISAInst::Enc29( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 30: inj_enc = new X86ISAInst::Enc30( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 31: inj_enc = new X86ISAInst::Enc31( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 32: inj_enc = new X86ISAInst::Enc32( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 33: inj_enc = new X86ISAInst::Enc33( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 34: inj_enc = new X86ISAInst::Enc34( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 35: inj_enc = new X86ISAInst::Enc35( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 36: inj_enc = new X86ISAInst::Enc36( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 37: inj_enc = new X86ISAInst::Enc37( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 38: inj_enc = new X86ISAInst::Enc38( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;
		case 39: inj_enc = new X86ISAInst::Enc39( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 
			break;		
		default: inj_enc = new X86ISAInst::Enc39( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); DPRINTF(csd, "WARNING:: Update time not handled by switch in cTXAlterMicroops()\n");
			break;
	}
	return inj_enc; 
}

StaticInstPtr MacroopBase::getInjInsn_EncFP(InstRegIndex dest){
		// ENC constructor originates from microregop.hh::8

	StaticInstPtr inj_enc; 
	int update_time=0;

        DPRINTF(csd, "WARNING:: UNIMPLEMENTED %d\n", curTick());


	//Placeholder
	// if(0 >= metadata->get_enc_latency()){
	// 	inj_enc = new X86ISAInst::EncNone( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 8, 0); 
	// 	return inj_enc;
	// }

	switch(update_time){
		case 0: inj_enc = new X86ISAInst::EncFP ( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 1: inj_enc = new X86ISAInst::Enc1FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 2: inj_enc = new X86ISAInst::Enc2FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);  	
			break;
		case 3: inj_enc = new X86ISAInst::Enc3FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);  	
			break;
		case 4: inj_enc = new X86ISAInst::Enc4FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);  	
			break;
		case 5: inj_enc = new X86ISAInst::Enc5FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   	
			break;
		case 6: inj_enc = new X86ISAInst::Enc6FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   	
			break;
		case 7: inj_enc = new X86ISAInst::Enc7FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   	
			break;
		case 8: inj_enc = new X86ISAInst::Enc8FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   	
			break;
		case 9: inj_enc = new X86ISAInst::Enc9FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0);   	
			break;
		case 10: inj_enc = new X86ISAInst::Enc10FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 11: inj_enc = new X86ISAInst::Enc11FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 12: inj_enc = new X86ISAInst::Enc12FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 13: inj_enc = new X86ISAInst::Enc13FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 14: inj_enc = new X86ISAInst::Enc14FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 15: inj_enc = new X86ISAInst::Enc15FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 16: inj_enc = new X86ISAInst::Enc16FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 17: inj_enc = new X86ISAInst::Enc17FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 18: inj_enc = new X86ISAInst::Enc18FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 19: inj_enc = new X86ISAInst::Enc19FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 20: inj_enc = new X86ISAInst::Enc20FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 21: inj_enc = new X86ISAInst::Enc21FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 22: inj_enc = new X86ISAInst::Enc22FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 23: inj_enc = new X86ISAInst::Enc23FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 24: inj_enc = new X86ISAInst::Enc24FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 25: inj_enc = new X86ISAInst::Enc25FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 26: inj_enc = new X86ISAInst::Enc26FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 27: inj_enc = new X86ISAInst::Enc27FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 28: inj_enc = new X86ISAInst::Enc28FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 29: inj_enc = new X86ISAInst::Enc29FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 30: inj_enc = new X86ISAInst::Enc30FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 31: inj_enc = new X86ISAInst::Enc31FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 32: inj_enc = new X86ISAInst::Enc32FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 33: inj_enc = new X86ISAInst::Enc33FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 34: inj_enc = new X86ISAInst::Enc34FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 35: inj_enc = new X86ISAInst::Enc35FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 36: inj_enc = new X86ISAInst::Enc36FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 37: inj_enc = new X86ISAInst::Enc37FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 38: inj_enc = new X86ISAInst::Enc38FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;
		case 39: inj_enc = new X86ISAInst::Enc39FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); 	
			break;		
		default: inj_enc = new X86ISAInst::Enc39FP( machInst, "INJ_ENC", (1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, dest, dest, dest, 4, 0); DPRINTF(csd, "WARNING:: Update time not handled by switch in cTXAlterMicroops()\n");
			break;
	}
	return inj_enc; 
}



std::vector<StaticInstPtr> 
MacroopBase::injectStoreMicros (StaticInstPtr store_microop){
	std::vector<StaticInstPtr> result;

    X86ISA::InstRegIndex dest = InstRegIndex(env.reg);
    X86ISA::InstRegIndex ptr = InstRegIndex(env.base);
	if (ptr.isZeroReg()){
		ptr = InstRegIndex(store_microop->srcRegIdx(1).index());
	}

	std::__cxx11::string diss = store_microop->generateDisassembly(0, NULL);
   	if(diss.find("_low") != std::string::npos){
		dest = InstRegIndex(FLOATREG_XMM_LOW(env.reg));
   	}
   	else if (diss.find("_high") != std::string::npos){
	   	dest = InstRegIndex(FLOATREG_XMM_HIGH(env.reg));
   	}

	std::string opName = store_microop->getName();


	//STORE constructor originates from microldstop.hh::100
	if(opName.compare("st")==0){
		StaticInstPtr inj_enc = getInjInsn_Enc(dest);
		inj_enc->setInjected();
		inj_enc->clearLastMicroop();
		result.push_back(inj_enc);

		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_FAUX_ST", 						//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				InstRegIndex(NUM_INTREGS),			// InstRegIndex _data //dest,
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		inj_load->setInjected();
		inj_load->clearLastMicroop();
		result.push_back(inj_load);

		StaticInstPtr existing_store = new X86ISAInst::St(
				machInst, 							//ExtMachInst _machInst
				"MOV_R_M",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				env.dataSize, 						//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_store->setInjected();
		existing_store->clearLastMicroop();
		result.push_back(existing_store);
	}
	else if(opName.compare("stfp")==0){
		StaticInstPtr inj_enc = getInjInsn_EncFP(dest);
		inj_enc->setInjected();
		inj_enc->clearLastMicroop();
		result.push_back(inj_enc);

		StaticInstPtr inj_load = new X86ISAInst::Ld(
				machInst, 							//ExtMachInst _machInst
				"INJ_FAUX_ST",						//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp() + 8,		// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				InstRegIndex(NUM_INTREGS),			// InstRegIndex _data //dest,
				4, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
        inj_load->setInjected();
        inj_load->clearLastMicroop();
        result.push_back(inj_load);


		StaticInstPtr existing_store = new X86ISAInst::Stfp(
				machInst, 							//ExtMachInst _machInst
				"MOV_R_M",							//const char * instMnem
				(1ULL << StaticInst::IsInjected) | (1ULL << StaticInst::IsMicroop) | 0, //uint64_t setFlags
				env.scale, 							// uint8_t _scale
				InstRegIndex(env.index), 			//InstRegIndex _index
				ptr, 								//InstRegIndex _base
				store_microop->getDisp(),			// uint64_t _disp
				InstRegIndex(env.seg), 				//InstRegIndex _segment
				dest,								// InstRegIndex _data
				8, 									//uint8_t _dataSize
				env.addressSize, 					//uint8_t _addressSize
				0); 								//Request::FlagsType _memFlags
		existing_store->clearLastMicroop();
		result.push_back(existing_store);
	}
	else{
		DPRINTF(csd, "WARNING:: OpName not handled by store injection in cTXAlterMicroops():: %s\n", opName);
	}

	return result;
}




int
MacroopBase::cTXAlterMicroops(bool arith_tainted, bool mem_tainted, Addr pc)
{
	if(ctx_decoded==false){
		
		DPRINTF(csd, "MacroopBase::cTXAlterMicroops():: Altering microops of tainted instruction 0x%x\n", pc);

		//Caculate total number of injected microops
		int num_inj_microops = 0;
		for(int i=0;i<numMicroops;i++){
			DPRINTF(csd, "(OLD %d)-- %s\n", i, microops[i]->generateDisassembly(0, NULL));
			if(microops[i]->isLoad() && mem_tainted)
			{
				num_inj_microops += countLoadMicros(microops[i]);
			}
			else if(microops[i]->isStore() && mem_tainted){
				num_inj_microops += countStoreMicros(microops[i]);
			}
			// else if(arith_tainted){
			// }


		}
		
		//Perform microop injection
		if(num_inj_microops > 0){
			
			int temp_numMicroops = numMicroops + num_inj_microops;
			int temp_index = 0;
			StaticInstPtr* temp_microops = new StaticInstPtr[temp_numMicroops];

			for(int i=0;i<numMicroops;i++){
				if(microops[i]->isLoad()) {
					std::vector<StaticInstPtr> toInject = injectLoadMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						temp_microops[temp_index+j] = toInject.at(j); 
					}
					temp_index = temp_index + toInject.size(); //Skip injected microops
				}
				else if(microops[i]->isStore()){
					std::vector<StaticInstPtr> toInject = injectStoreMicros(microops[i]);
					for(int j=0; j<toInject.size(); j++){
						temp_microops[temp_index+j] = toInject.at(j); 
					}
					temp_index = temp_index + toInject.size(); //Skip injected microops	
				}
				else {
					temp_microops[temp_index]=microops[i];
					temp_microops[temp_index]->clearLastMicroop();
					temp_index++;
				}
			}
			delete [] microops;
			microops = temp_microops;
			numMicroops = temp_numMicroops;
			microops[(numMicroops-1)]->setLastMicroop();
		}
		ctx_decoded=true;
	
		for(int i=0; i<numMicroops; i++){
        		DPRINTF(csd, "(NEW %d)-- %s\n", i, microops[i]->generateDisassembly(0, NULL));
		}
	}
	return 0;

}




}


