#include "cpu.hpp"

CPU* CPU::instancePtr = nullptr;
std::mutex CPU::mtx;

void CPU::Tick(){

    //Snapshot of previous state
    TempOut previousTemp = TemporaryValues::GetInstance()->GetValues();
    RegsOut previousRegs = RegisterFile::GetInstance()->GetRegsValues();
    CU_Data previousControlUnitData = FetchControlUnitData();
    ALU_Data previousALUData = PerformALUOperations(previousControlUnitData);
    uint16_t oldRBValue = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(previousControlUnitData.srcRB));
    uint16_t oldRAValue = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(previousControlUnitData.srcRA));
    uint16_t oldIR0Data = previousRegs.IR0;
    uint16_t oldIR1Data = previousRegs.IR1;

    //Current state
    Clock::GetInstance()->Increment();
    bool currentClockSignal = Clock::GetInstance()->GetClockSignal(previousControlUnitData.HLT);
    bool regWrite = previousControlUnitData.regWrite & (previousTemp.isCurrExt | ((previousControlUnitData.ALU_DATA & 0b10000) >> 4) | previousTemp.regIsCurrAddr | previousControlUnitData.useIn);

    //Clock Edge (Executed DURING edge (so we can only use old/previous values))
    TempOut newTempValues = UpdateTemporaryValuesOnClock(previousControlUnitData, previousTemp, currentClockSignal);
    RegsOutOnChange newRegsValues = UpdateRegistersOnClock(previousControlUnitData, previousTemp, previousRegs, 
                                                            previousALUData, oldRAMvalue, currentClockSignal, newTempValues, regWrite);
    
    UpdateRAMOnClock(previousControlUnitData, previousTemp, previousRegs,
                        currentClockSignal, oldRBValue, oldRAValue, regWrite);


    CU_Data newControlUnitData = FetchControlUnitData();
    RegsOut newRegsOut = RegisterFile::GetInstance()->GetRegsValues();
    TempOut newTempOut = TemporaryValues::GetInstance()->GetValues();
    uint16_t newRBValue = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(newControlUnitData.srcRB));
    uint16_t newRAValue = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(newControlUnitData.srcRA));
    MI_Data miData = MemoryInterface::GetInstance()->GetMI_Data(newControlUnitData, newRegsOut, newTempOut, currentClockSignal, 0, newRBValue, newRAValue, false);
    uint16_t newRAMValue = RAM::GetInstance()->Read(miData.RAM_ADDRESS);

    //Clock Idle (Executed AFTER edge (so we can use new values))
    RegsOutOnIdle regsOutOnClockIdle = UpdateRegistersOnIdle(newTempValues, newRAMValue, oldIR0Data, oldIR1Data, currentClockSignal);

    if(newControlUnitData.useOut)
        SetOUT(newControlUnitData.ioPort, newRAValue);

    //Finalize (update graphics)
    UpdateVisualRAMCurrentAddress(oldRAMAddress, miData.RAM_ADDRESS);
    oldRAMAddress = miData.RAM_ADDRESS;
    oldRAMvalue = newRAMValue;
}

CU_Data CPU::FetchControlUnitData()
{
    uint16_t ir0Val = RegisterFile::GetInstance()->GetRegValue(IR0);
    uint16_t flagsVal = RegisterFile::GetInstance()->GetRegValue(FLAGS);
    return ControlUnit::GetInstance()->GetCU_Data(ir0Val, flagsVal);
}

ALU_Data CPU::PerformALUOperations(const CU_Data& controlUnitData)
{
    uint16_t srcAVal = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(controlUnitData.srcRA));
    uint16_t srcBVal = RegisterFile::GetInstance()->GetRegValue(static_cast<RegisterName>(controlUnitData.srcRB));

    uint8_t aluOpcode = controlUnitData.ALU_DATA & 0b01111;
    bool writeBackFlag = controlUnitData.ALU_DATA & 0b10000;

    return ALU::GetInstance()->GetALU_Data(srcAVal, srcBVal, aluOpcode, writeBackFlag);
}

TempOut CPU::UpdateTemporaryValuesOnClock(const CU_Data& oldControlUnitData, const TempOut& oldTemporaryValues, bool currentClockSignal)
{
    TempIn tempIn = {0};
    tempIn.clockSignal = currentClockSignal;
    tempIn.isNxtExt = oldControlUnitData.isNxtExt & !oldTemporaryValues.isCurrExt;
    tempIn.jsr = oldControlUnitData.jsr & !oldTemporaryValues.isCurrJsr;
    tempIn.containsAddress = tempIn.jsr | (oldControlUnitData.containsAddress & oldTemporaryValues.isCurrExt);
    tempIn.rts = oldControlUnitData.rts;
    tempIn.spChange = oldControlUnitData.spChange & !oldTemporaryValues.isCurrSpChange & !oldTemporaryValues.isCurrExt;
    tempIn.regIsAddr = oldControlUnitData.regIsAddress & !oldTemporaryValues.regIsCurrAddr;
    tempIn.isCurrExt = oldTemporaryValues.isCurrExt;

    return TemporaryValues::GetInstance()->OnClockChange(tempIn);
}

RegsOutOnChange CPU::UpdateRegistersOnClock(CU_Data oldControlUnitData, const TempOut& oldTemporaryValues, const RegsOut& oldRegsOut, 
                                           const ALU_Data& oldAluData, uint16_t oldRAM_OUT, bool currentClockSignal, const TempOut& newTempValues, bool regWrite)
{
    // Prepare regsOutOnChangeTemp for source registers (srcRA/srcRB)
    RegsOutOnChange regsOutOnChangeTemp = {};
    static const int NUM_REGISTERS = 8;
    const uint16_t* regValues[NUM_REGISTERS] = {
        &oldRegsOut.R0, &oldRegsOut.R1, &oldRegsOut.R2, &oldRegsOut.R3,
        &oldRegsOut.R4, &oldRegsOut.R5, &oldRegsOut.R6, &oldRegsOut.R7
    };

    uint8_t srcRAIndex = static_cast<uint8_t>(oldControlUnitData.srcRA);
    uint8_t srcRBIndex = static_cast<uint8_t>(oldControlUnitData.srcRB);

    if (srcRAIndex < NUM_REGISTERS) {
        regsOutOnChangeTemp.RA = *regValues[srcRAIndex];
    }
    if (srcRBIndex < NUM_REGISTERS) {
        regsOutOnChangeTemp.RB = *regValues[srcRBIndex];
    }
    regsOutOnChangeTemp.SP = oldRegsOut.SP;
    regsOutOnChangeTemp.PC = oldRegsOut.PC;
    regsOutOnChangeTemp.FLAGS = oldRegsOut.FLAGS;
    regsOutOnChangeTemp.RAM_ADDRESS = oldRegsOut.RAM_ADDRESS;

    RegsInOnChange regsInOnClockChange = {};
    regsInOnClockChange.carry = oldAluData.carry;
    regsInOnClockChange.flagsClock = currentClockSignal;
    regsInOnClockChange.flagsWrite = oldControlUnitData.flagsWrite;
    regsInOnClockChange.gpClock = oldTemporaryValues.isCurrAddr ? !currentClockSignal : currentClockSignal;

    uint16_t ioDataIn = GetIN(oldControlUnitData.ioPort);

    regsInOnClockChange.gpData = oldControlUnitData.useIn ? ioDataIn : (oldTemporaryValues.isCurrSpChange | oldTemporaryValues.regIsCurrAddr) ? oldRAM_OUT : (oldTemporaryValues.isCurrExt ? oldRegsOut.IR1 : oldAluData.result);    
    regsInOnClockChange.gpRegToWrite = oldControlUnitData.dstR;
    regsInOnClockChange.gpRegWrite = regWrite;
    regsInOnClockChange.negative = oldAluData.negative;
    regsInOnClockChange.overflow = oldAluData.overflow;

    ///////////////////////////////////////////////////////////
    //////// DOOMED ZONE //////////////////////////////////////
    ///////////////////////////////////////////////////////////

    bool regIsAddress = false;

    if(!currentClockSignal){
        uint8_t OpCode = (oldRAM_OUT >> 13) & 0b111;
        uint8_t subOpCode = (oldRAM_OUT >> 9) & 0b1111;
        regIsAddress = (OpCode == 3 && subOpCode == 2);
    }

    bool REG_IS_ADDR = regIsAddress && !oldTemporaryValues.regIsCurrAddr;
    bool SP_CHANGE = oldControlUnitData.spChange & !oldTemporaryValues.isCurrExt & !oldTemporaryValues.isCurrSpChange;

    regsInOnClockChange.pcClock = currentClockSignal & !newTempValues.isCurrJsr & !newTempValues.isCurrSpChange & 
                                    !SP_CHANGE & !REG_IS_ADDR & !oldTemporaryValues.regIsCurrAddr;

    /// END OF DOOMED ZONE ///

    regsInOnClockChange.pcData = (oldControlUnitData.rts) ? oldRAM_OUT : oldRegsOut.IR1;
    regsInOnClockChange.ramAddrClock = !currentClockSignal;
    regsInOnClockChange.raRead = oldControlUnitData.srcRA;
    regsInOnClockChange.rbRead = oldControlUnitData.srcRB;
    regsInOnClockChange.spClock = oldTemporaryValues.isCurrSpChange & !currentClockSignal;
    regsInOnClockChange.spPop = oldControlUnitData.spPop;
    regsInOnClockChange.writeToPC = ((oldControlUnitData.loadPC & oldTemporaryValues.isCurrExt) & !oldTemporaryValues.isCurrJsr & oldTemporaryValues.isCurrAddr) | oldControlUnitData.rts;
    regsInOnClockChange.zero = oldAluData.zero;

    return RegisterFile::GetInstance()->OnClockChange(regsInOnClockChange);
}

void CPU::UpdateRAMOnClock(const CU_Data& oldControlUnitData, const TempOut& oldTemporaryValues, const RegsOut& oldRegsOut, bool currentClockSignal, uint16_t oldRBValue, uint16_t oldRAValue, bool regWrite)
{
    bool memWrite = (oldTemporaryValues.isCurrSpChange & !oldControlUnitData.spPop) | (oldControlUnitData.memWrite & (oldTemporaryValues.isCurrExt | oldTemporaryValues.regIsCurrAddr));

    MI_Data miData = MemoryInterface::GetInstance()->GetMI_Data(oldControlUnitData, oldRegsOut, oldTemporaryValues, currentClockSignal, memWrite, oldRBValue, oldRAValue, regWrite);

    MemoryInterface::GetInstance()->OnClockChange(miData);
}

RegsOutOnIdle CPU::UpdateRegistersOnIdle(const TempOut& newtempValues, uint16_t newRamValue, uint16_t ir0Data, uint16_t ir1Data, bool currentClockSignal)
{
    RegsInOnIdle regInOnClockIdle = {};

    regInOnClockIdle.ir0Clock = currentClockSignal || newtempValues.isCurrExt || newtempValues.regIsCurrAddr;
    regInOnClockIdle.ir1Clock = newtempValues.isCurrExt;
    regInOnClockIdle.ir0Write = !newtempValues.isCurrSpChange;

    regInOnClockIdle.ir0Data = newtempValues.isCurrExt ? ir0Data : newRamValue;
    regInOnClockIdle.ir1Data = newtempValues.isCurrExt ? newRamValue : ir1Data;

    return RegisterFile::GetInstance()->OnClockIdle(regInOnClockIdle);
}

void CPU::RunFrame()
{
    const int ticks_per_frame = Clock::GetInstance()->GetFrequency() * 10;
    if(ticks_per_frame > 0){
        for (int i = 0; i < ticks_per_frame; ++i) {
            Tick();
        }
    }
    else{
        Tick();
    }
}

void CPU::Init(){
    RAM::GetInstance();
    RegisterFile::GetInstance();
    MemoryInterface::GetInstance();
    TemporaryValues::GetInstance();
    ALU::GetInstance();
    Clock::GetInstance();

    uint16_t RAM0 = RAM::GetInstance()->Read(0);
    RegisterFile::GetInstance()->SetRegValue(IR0, RAM0);
    oldRAMvalue = RAM0;


    uint8_t ALU_exec_infos = 0;

    {
        const uint8_t OpCode = (RAM0 >> 13) & 0b111;
        const uint8_t subOpCode = (RAM0 >> 9) & 0b1111;

        if(OpCode == 0){
            ALU_exec_infos = 16 + subOpCode;
        }
        else if(OpCode == 0b001 && (subOpCode == 10 || subOpCode == 11)){
            ALU_exec_infos = 26 + (subOpCode - 10);
        } 
    }

    ALU_Data ALU_OUT = ALU::GetInstance()->GetALU_Data(0, 0, ALU_exec_infos & 0b01111, ALU_exec_infos & 0b10000);

    
    RegisterFile::GetInstance()->SetRegValue(SP, 0xFFFF);
    
    UpdateVisualRAMCurrentAddress(oldRAMAddress, 0);
    ResetIOPortsVisual();
    oldRAMAddress = 0;
}

void CPU::Reset(){
    Clock::GetInstance()->Reset();
    TemporaryValues::GetInstance()->Reset();
    RegisterFile::GetInstance()->Reset();
    MemoryInterface::GetInstance()->Reset();
    TemporaryValues::GetInstance()->ProcessFlipflopsAndUpdateDebug(TemporaryValues::GetInstance()->flipflops);
    Init();
}