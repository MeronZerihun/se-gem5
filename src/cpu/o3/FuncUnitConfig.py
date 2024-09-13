# Copyright (c) 2010, 2017 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2006-2007 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Kevin Lim

from m5.SimObject import SimObject
from m5.defines import buildEnv
from m5.params import *

from m5.objects.FuncUnit import *

AES128_LATENCY    = 40
AES128_NI_LATENCY = 70
SIMON128_LATENCY  = 20
QARMA128_LATENCY  = 12
QARMA192_LATENCY  = 14
QARMA256_LATENCY  = 16
HASH_LATENCY      = 1

ENC_LATENCY = QARMA128_LATENCY
# DEC_LATENCY = 2 * QARMA128_LATENCY

#### BEGIN EMTD
class Encrypt(FUDesc):
    opList = [ OpDesc(opClass='Encrypt', opLat=ENC_LATENCY, pipelined=True),
    OpDesc(opClass='Encrypt1', opLat=ENC_LATENCY-1, pipelined=True),
    OpDesc(opClass='Encrypt2', opLat=ENC_LATENCY-2,  pipelined=True),
    OpDesc(opClass='Encrypt3', opLat=ENC_LATENCY-3,  pipelined=True),
    OpDesc(opClass='Encrypt4', opLat=ENC_LATENCY-4,  pipelined=True),
    OpDesc(opClass='Encrypt5', opLat=ENC_LATENCY-5,  pipelined=True),
    OpDesc(opClass='Encrypt6', opLat=ENC_LATENCY-6,  pipelined=True),
    OpDesc(opClass='Encrypt7', opLat=ENC_LATENCY-7,  pipelined=True),
    OpDesc(opClass='Encrypt8', opLat=ENC_LATENCY-8,  pipelined=True),
    OpDesc(opClass='Encrypt9', opLat=ENC_LATENCY-9,  pipelined=True),
    OpDesc(opClass='Encrypt10', opLat=ENC_LATENCY-10, pipelined=True),
    OpDesc(opClass='Encrypt11', opLat=ENC_LATENCY-11,  pipelined=True),
    # OpDesc(opClass='Encrypt12', opLat=ENC_LATENCY-12,  pipelined=True),
    # OpDesc(opClass='Encrypt13', opLat=ENC_LATENCY-13,  pipelined=True),
    # OpDesc(opClass='Encrypt14', opLat=ENC_LATENCY-14,  pipelined=True),
    # OpDesc(opClass='Encrypt15', opLat=ENC_LATENCY-15,  pipelined=True),
    # OpDesc(opClass='Encrypt16', opLat=ENC_LATENCY-16,  pipelined=True),
    # OpDesc(opClass='Encrypt17', opLat=ENC_LATENCY-17,  pipelined=True),
    # OpDesc(opClass='Encrypt18', opLat=ENC_LATENCY-18,  pipelined=True),
    # OpDesc(opClass='Encrypt19', opLat=ENC_LATENCY-19,  pipelined=True),
    #OpDesc(opClass='Encrypt20', opLat=ENC_LATENCY-20, pipelined=True),
    #OpDesc(opClass='Encrypt21', opLat=ENC_LATENCY-21,  pipelined=True),
    #OpDesc(opClass='Encrypt22', opLat=ENC_LATENCY-22,  pipelined=True),
    #OpDesc(opClass='Encrypt23', opLat=ENC_LATENCY-23,  pipelined=True),
    #OpDesc(opClass='Encrypt24', opLat=ENC_LATENCY-24,  pipelined=True),
    #OpDesc(opClass='Encrypt25', opLat=ENC_LATENCY-25,  pipelined=True),
    #OpDesc(opClass='Encrypt26', opLat=ENC_LATENCY-26,  pipelined=True),
    #OpDesc(opClass='Encrypt27', opLat=ENC_LATENCY-27,  pipelined=True),
    #OpDesc(opClass='Encrypt28', opLat=ENC_LATENCY-28,  pipelined=True),
    #OpDesc(opClass='Encrypt29', opLat=ENC_LATENCY-29,  pipelined=True),
    #OpDesc(opClass='Encrypt30', opLat=ENC_LATENCY-30, pipelined=True),
    #OpDesc(opClass='Encrypt31', opLat=ENC_LATENCY-31,  pipelined=True),
    #OpDesc(opClass='Encrypt32', opLat=ENC_LATENCY-32,  pipelined=True),
    #OpDesc(opClass='Encrypt33', opLat=ENC_LATENCY-33,  pipelined=True),
    #OpDesc(opClass='Encrypt34', opLat=ENC_LATENCY-34,  pipelined=True),
    #OpDesc(opClass='Encrypt35', opLat=ENC_LATENCY-35,  pipelined=True),
    #OpDesc(opClass='Encrypt36', opLat=ENC_LATENCY-36,  pipelined=True),
    #OpDesc(opClass='Encrypt37', opLat=ENC_LATENCY-37,  pipelined=True),
    #OpDesc(opClass='Encrypt38', opLat=ENC_LATENCY-38,  pipelined=True),
    #OpDesc(opClass='Encrypt39', opLat=ENC_LATENCY-39,  pipelined=True),
    OpDesc(opClass='EncryptNone',  opLat=1, pipelined=True)
    ]

    count = 1

class Decrypt(FUDesc):
    opList = [
    OpDesc(opClass='Decrypt', opLat=ENC_LATENCY, pipelined=True),
    OpDesc(opClass='DecryptHit', opLat=1,             pipelined=True)
    ]

    count = 1
#### END EMTD

class IntALU(FUDesc):
    opList = [  OpDesc(opClass='IntAlu') ]
    count = 6

class Enc_IntALU(FUDesc):
    opList = [  OpDesc(opClass='EncIntAlu', opLat=max(1, HASH_LATENCY)) ]
    count = 6

class IntMultDiv(FUDesc):
    opList = [ OpDesc(opClass='IntMult', opLat=3),
               OpDesc(opClass='IntDiv', opLat=20, pipelined=False) ]

    # DIV and IDIV instructions in x86 are implemented using a loop which
    # issues division microops.  The latency of these microops should really be
    # one (or a small number) cycle each since each of these computes one bit
    # of the quotient.
    if buildEnv['TARGET_ISA'] in ('x86'):
        opList[1].opLat=max(1, HASH_LATENCY)

    count=2

class Enc_IntMultDiv(FUDesc):
    opList = [
    OpDesc(opClass='EncIntMult', opLat=max(3, HASH_LATENCY)),
    OpDesc(opClass='EncIntDiv', opLat=max(20, HASH_LATENCY), pipelined=False) ]
    count=2


class FP_ALU(FUDesc):
    opList = [ OpDesc(opClass='FloatAdd',    opLat=2),
               OpDesc(opClass='FloatCmp',    opLat=2),
               OpDesc(opClass='FloatCvt',    opLat=2)]
    count = 4

class Enc_FP_ALU(FUDesc):
    opList = [ OpDesc(opClass='EncFloatAdd', opLat=max(2, HASH_LATENCY)),
               OpDesc(opClass='EncFloatCmp', opLat=max(2, HASH_LATENCY)),
               OpDesc(opClass='EncFloatCvt', opLat=max(2, HASH_LATENCY))  ]
    count = 4

class FP_MultDiv(FUDesc):
    opList = [ OpDesc(opClass='FloatMult', opLat=4),
               OpDesc(opClass='FloatMultAcc', opLat=5),
               OpDesc(opClass='FloatMisc', opLat=3),
               OpDesc(opClass='FloatDiv', opLat=12, pipelined=False),
               OpDesc(opClass='FloatSqrt', opLat=24, pipelined=False) ]
    count = 2

class Enc_FP_MultDiv(FUDesc):
    opList = [
    OpDesc(opClass='EncFloatMult', opLat=max(4, HASH_LATENCY)),
    OpDesc(opClass='EncFloatMultAcc', opLat=max(5, HASH_LATENCY)),
    OpDesc(opClass='EncFloatMisc', opLat=max(3, HASH_LATENCY)),
    OpDesc(opClass='EncFloatDiv', opLat=max(12, HASH_LATENCY),pipelined=False),
    OpDesc(opClass='EncFloatSqrt',opLat=max(24, HASH_LATENCY),pipelined=False)]
    count = 2


class SIMD_Unit(FUDesc):
    opList = [ OpDesc(opClass='SimdAdd'),
               OpDesc(opClass='SimdAddAcc'),
               OpDesc(opClass='SimdAlu'),
               OpDesc(opClass='SimdCmp'),
               OpDesc(opClass='SimdCvt'),
               OpDesc(opClass='SimdMisc'),
               OpDesc(opClass='SimdMult'),
               OpDesc(opClass='SimdMultAcc'),
               OpDesc(opClass='SimdShift'),
               OpDesc(opClass='SimdShiftAcc'),
               OpDesc(opClass='SimdDiv'),
               OpDesc(opClass='SimdSqrt'),
               OpDesc(opClass='SimdFloatAdd'),
               OpDesc(opClass='SimdFloatAlu'),
               OpDesc(opClass='SimdFloatCmp'),
               OpDesc(opClass='SimdFloatCvt'),
               OpDesc(opClass='SimdFloatDiv'),
               OpDesc(opClass='SimdFloatMisc'),
               OpDesc(opClass='SimdFloatMult'),
               OpDesc(opClass='SimdFloatMultAcc'),
               OpDesc(opClass='SimdFloatSqrt'),
               OpDesc(opClass='SimdReduceAdd'),
               OpDesc(opClass='SimdReduceAlu'),
               OpDesc(opClass='SimdReduceCmp'),
               OpDesc(opClass='SimdFloatReduceAdd'),
               OpDesc(opClass='SimdFloatReduceCmp')
               ]
    count = 4

class Enc_SIMD_Unit(FUDesc):
    opList = [
        OpDesc(opClass='EncSimdAdd', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdAddAcc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdAlu', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdCmp', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdCvt', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdMisc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdMult', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdMultAcc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdShift', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdShiftAcc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdDiv', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdSqrt', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatAdd', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatAlu', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatCmp', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatCvt', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatDiv', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatMisc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatMult', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatMultAcc', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatSqrt', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdReduceAdd', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdReduceAlu', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdReduceCmp', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatReduceAdd', opLat=max(1, HASH_LATENCY)),
        OpDesc(opClass='EncSimdFloatReduceCmp', opLat=max(1, HASH_LATENCY))
        ]
    count = 4



class PredALU(FUDesc):
    opList = [ OpDesc(opClass='SimdPredAlu') ]
    count = 1

class ReadPort(FUDesc):
    opList = [ OpDesc(opClass='MemRead'),
               OpDesc(opClass='FloatMemRead') ]
    count = 0

class WritePort(FUDesc):
    opList = [ OpDesc(opClass='MemWrite'),
               OpDesc(opClass='FloatMemWrite') ]
    count = 0

class RdWrPort(FUDesc):
    opList = [ OpDesc(opClass='MemRead'), OpDesc(opClass='MemWrite'),
               OpDesc(opClass='FloatMemRead'), OpDesc(opClass='FloatMemWrite')]
    count = 4

class IprPort(FUDesc):
    opList = [ OpDesc(opClass='IprAccess', pipelined = False) ]
    count = 1
