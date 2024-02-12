#ifndef CPU_H
#define CPU_H

#include <vector>
#include <iostream>
#include <math.h>
#include <fstream>
#include <string>
#include <bitset>

#define NUM_REGS 32
#define NUM_MEMS 32
#define AND 0
#define OR 1
#define ADD 2
#define SUB 6
#define SLT 7
#define NOR 12
#define R_TYPE 0
#define LW 35
#define SW 43
#define BEQ 4
#define J 2
#define JAL 3
#define JR 100

#define FUNC_ADD 32
#define FUNC_SUB 34
#define FUNC_AND 36
#define FUNC_OR 37
#define FUNC_SLT 42
#define FUNC_NOR 39
#define FUNC_JR 8

#define RA 31

class CPU
{
private:
    int total_clock_cycles, pc;
    int next_pc, branch_target, jump_target;
    int alu_op, alu_zero;
    int jump, regdest, alusrc, memtoreg, regwrite, memread, memwrite, branch, jlink; // jlink is a control signal I created to handle JAL instruction
    bool insttype1, insttype0; // used in ALU control to determine ALU op for most non-R-type instructions
    int writeToReg; // destination register, will be either rs or rd, depending on mux. determined in decode and used in writeback
    int regtopc; // pc = $ra, custom control signal enabled by alucontrol for JR instruction
    std::vector<int> registerfile;
    std::vector<std::string> registerfile_name; // stores the names of the register files in their respective indices
    std::pair<bool, int> touched_register;  // stores register that was modified in current cycle and the new value stored there
    std::vector<int> d_mem;
    std::vector<int> d_mem_address; // stores memory addresses at their respective indices
    std::pair<bool, int> touched_memory; // stores memory addresses that was modified in current cycle and the new value stored there

    void decode(std::string str);
    void execute(int signextend, int rs, int rt, int funct);
    void mem(int result, int rt);
    void writeback(int result, int d_memval);
    void controlunit(int opcode);
    void alucontrol(int insttype, int funct);
    int signExtended(int signextend); // used to sign extend immediate value
    int shiftLeftTwo(int val); // performs shift left 2
    int fourMostSigFromPC(); //obtains four most significant bits from PC and returns that as an int

    int convertBinaryStrToInt(std::string); // used primarily in decode to get integer value from the input string for each field

    void resetTouchedRegister();    //resets member variable that listens to what register was touched
    void resetTouchedMemory();     //resets member variable that listens to what memory address was touched

public:
    CPU();

    bool fetch(std::string filename);

    int getTotalClockCycles();
    int getPC();
    bool registerWasTouched();
    bool memoryWasTouched();
    void updateRegisterFile(int i, int val);
    void updateMemory(int i, int val);
    void printTouchedRegister();
    void printTouchedMemory();
};

CPU::CPU()
{
    pc = next_pc = branch_target = jump_target = 0;
    alu_op = alu_zero = 0;

    // register file array initialized to have all zeros
    registerfile.resize(NUM_REGS, 0);

    registerfile_name = { "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3", 
                            "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", 
                            "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7", 
                            "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra" };

    touched_register = { false, 0 };

    d_mem.resize(NUM_MEMS, 0);

    // initializes all possible d_mem addresses
    for (int i = 0; i < NUM_MEMS; i++)
    {
        d_mem_address.push_back(i * 4);
    }

    touched_memory = { false, 0 };

    total_clock_cycles = 0;

    jump = regdest = alusrc = memtoreg = regwrite = memread = memwrite = branch = jlink = 0;

    writeToReg = 0;

    regtopc = 0;
}

// fetch instruction from within the fetch() function
bool CPU::fetch(std::string filename)
{
    // resets touched registers and memory
    resetTouchedRegister();
    resetTouchedMemory();

    int instruction_counter = 0;
    std::string str;
    std::ifstream myfile(filename);

    // loops through input file until it reaches the instruction it should read (according to PC)
    while (instruction_counter <= pc / 4)
    {
        if (std::getline(myfile, str))
        {
            instruction_counter++;
        }
        else 
        {
            return false;
        }
    }

    next_pc = pc + 4;

    decode(str);

    // if JR instruction, regtopc control signal will be enabled, preventing pc = $ra from being overwritten here
    if (regtopc)
    {
        total_clock_cycles++;
        return true;
    }
    // otherwise, pc is set accordingly
    else
    {
        if (jump)
        {
            // excludes jlink because pc is already set and clock cycles already incremented
            if (!jlink)
            {
                pc = jump_target;
                total_clock_cycles++;
            }
        }
        else
        {
            if (alu_zero && branch)
            {
                pc = branch_target;
            }
            else
            {
                pc = next_pc;
            }
        }
    }

    return true;
}

void CPU::decode(std::string str)
{
    int opcode = convertBinaryStrToInt(str.substr(0, 6));
    int rs = convertBinaryStrToInt(str.substr(6, 5));
    int rt = convertBinaryStrToInt(str.substr(11, 5));
    int rd = convertBinaryStrToInt(str.substr(16, 5));
    int shamt = convertBinaryStrToInt(str.substr(21, 5));
    int funct = convertBinaryStrToInt(str.substr(26, 6));
    int imm = convertBinaryStrToInt(str.substr(16, 16));
    int addr = convertBinaryStrToInt(str.substr(6, 26));

    controlunit(opcode);

    regtopc = 0; // if jr, then alucontrol will flip this on
    
    // mux before write reg
    if (regdest)
    {
        // write reg is 15-11 (Rd)
        writeToReg = rd;
    }
    else
    {
        // write reg is 20-16 (Rt)
        writeToReg = rt;
    }

    // sign extension unit
    imm = signExtended(imm);

    jump_target = shiftLeftTwo(addr) + fourMostSigFromPC();

    if (jump) 
    {
        if (jlink)
        {
            writeToReg = RA;
            writeback(next_pc, 999); //second param wont be used, so 999 is dummy value
            pc = jump_target;
            total_clock_cycles++;
        }
        return;
    }

    execute(imm, registerfile[rs], registerfile[rt], funct);
}

void CPU::execute(int signextend, int rs, int rt, int funct)
{
    int insttype = insttype1 * pow(2, 1) + insttype0 * pow(2, 0);

    alucontrol(insttype, funct);

    // this control signal is generated by the alucontrol 
    // only when a JR instruction's funct is passed to it
    if (regtopc)
    {
        pc = rs;
        return;
    }

    int alu_result;

    int alu_operand1;
    int alu_operand2;
    
    alu_operand1 = rs;

    // mux before alu_operand2
    if (alusrc)
    {
        alu_operand2 = signextend;
    }
    else 
    {
        alu_operand2 = rt;
    }

    // alu_op
    if (alu_op == AND) 
    {
        alu_result = alu_operand1 & alu_operand2;
    }
    else if (alu_op == OR) 
    {
        alu_result = alu_operand1 | alu_operand2;
    }
    else if (alu_op == ADD) 
    {
        alu_result = alu_operand1 + alu_operand2;
    }
    else if (alu_op == SUB) 
    {
        alu_result = alu_operand1 - alu_operand2;
    }
    else if (alu_op == SLT)
    {
        alu_result = (alu_operand1 < alu_operand2) ? 1 : 0;
    }
    else if (alu_op == NOR) 
    {
        alu_result = ~(alu_operand1 | alu_operand2);
    }

    if (alu_result == 0)
    {
        alu_zero = 1;

        if (branch)
        {
            branch_target = shiftLeftTwo(signextend);
            branch_target += next_pc; // adder

            total_clock_cycles++;

            return;
        }
    }
    else
    {
        alu_zero = 0;
    }

    // calculate the branch target address
    // take the 26-bit input from fetch?
    // Run shift-left-2 (you may want to use multiplication)
    // merge it with the first 4 bits of the next_pc 
    
    mem(alu_result, rt);
}

void CPU::mem(int alu_result, int rt)
{
    // Mem() function should receive memory address for both LW and SW instructions

    // For the LW, the value stored in the indicated d-mem array entry should be retrieved and sent to the Writeback()
    if (memread) // if control value signals LW
    {
        int address = alu_result;
        int dataAtMemoryIndex = address / 4; //shitty name
        writeback(alu_result, d_mem[dataAtMemoryIndex]); // guess I will after all lol ~~wanted to pass mem vector value generated from here but wont for now~~
    }
    else if (memwrite)
    {
        int address = alu_result;
        int dataAtMemoryIndex = address / 4;
        d_mem[dataAtMemoryIndex] = rt;

        touched_memory = { true, dataAtMemoryIndex };

        total_clock_cycles++;
    }
    else // only alu_result should be passed into writeback()
    {
        writeback(alu_result, 999); // memread is off so no second value will be passed in to the mux in writeback. I use a dummy variable 999 as a paramter, but it will never be used in its datapath
    }
}

void CPU::writeback(int result, int d_memval)
{
    // get the computation results from ALU and a data from data memory 
    // update the destination register in the registerfile array. (if a load word)

    // checks if a writeback will actually occur
    if (regwrite)
    {
        // mux
        if (memtoreg)
        {
            //then write back to register from memory
            registerfile[writeToReg] = d_memval;
            touched_register = { true, writeToReg };
        }
        else 
        {
            if (jlink)
            {
                registerfile[writeToReg] = next_pc;
                touched_register = { true, writeToReg };
                return;
            }
            else
            {
                //then write back to register from alu operation
                registerfile[writeToReg] = result;
                touched_register = { true, writeToReg };
            }
        }
    }

    // cycle count should be incremented.
    total_clock_cycles++;
}

void CPU::controlunit(int opcode)
{
    //will receive 6-bit opcode value and generate 9 control signals 
    // can be seen on page 3 of lecture slide “CSE140_Lecture-4_Processor-3”
    if (opcode == R_TYPE)
    {
        regdest = regwrite = insttype1 = 1;
        jump = alusrc = memtoreg = memread = memwrite = branch = insttype0 = jlink = 0;
    }
    else if (opcode == LW)
    {
        alusrc = memtoreg = regwrite = memread = 1;
        jump = regdest = memwrite = branch = insttype1 = insttype0 = jlink = 0;
    }
    else if (opcode == SW)
    {       
        alusrc = memwrite = 1;
        jump = regwrite = memread = branch = insttype1 = insttype0 = jlink = 0;
    }
    else if (opcode == BEQ)
    {
        branch = insttype0 = 1;
        jump = alusrc = regwrite = memread = memwrite = insttype1 = jlink = 0;
    }
    else if (opcode == J)
    {
        jump = 1;
        regwrite = memread = memwrite = branch = jlink = 0;
    }
    else if (opcode == JAL)
    {
        jump = regwrite = jlink = 1;
        memread = memwrite = branch = 0;
    }
}

void CPU::alucontrol(int insttype, int func)
{
    // generates ALU OP input for the Execute() function.
    if (insttype == 0)
    {
        alu_op = ADD;
    }
    else if (insttype == 1)
    {
        alu_op = SUB;
    }
    else if (insttype == 2)
    {
        if (func == FUNC_ADD)
        {
            alu_op = ADD;
        }
        else if (func == FUNC_SUB)
        {
            alu_op = SUB;
        }
        else if (func == FUNC_AND)
        {
            alu_op = AND;
        }
        else if (func == FUNC_OR)
        {
            alu_op = OR;
        }
        else if (func == FUNC_SLT)
        {
            alu_op = SLT;
        }
        else if (func == FUNC_NOR)
        {
            alu_op = NOR;
        }
        else if (func == FUNC_JR)
        {
            alu_op = JR;
            regtopc = 1;
        }
    }
}

int CPU::signExtended(int signextend)
{
    int new_val = 0;

    // int to 32 bit binary val
    std::bitset<16> bt(signextend);

    // string representation of binary val
    std::string bin_str = bt.to_string();

    if (bin_str.substr(0, 1) == "1")
    {
        bin_str = "1111111111111111" + bin_str;
    }
    else 
    {
        bin_str = "0000000000000000"  + bin_str;
    }

    //strip off leading bit and convert to int
    int leading_bit_stripped_off = convertBinaryStrToInt(bin_str.substr(1, bin_str.length() - 1));
    
    // account for leading bit if 1
    if (bin_str.substr(0,1) == "1")
    {
        int leading_bit = -1 * pow(2, bin_str.length() - 1);
        new_val = leading_bit + leading_bit_stripped_off;
    }
    // ignore leading bit if 0
    else
    {
        new_val = leading_bit_stripped_off;
    }

    return new_val;
}

// performs val * 4;
int CPU::shiftLeftTwo(int val)
{
    return val << 2;
}

int CPU::convertBinaryStrToInt(std::string str)
{
	int val = 0;

	for (int i = 0; i < str.length(); i++)
	{
		val += std::stoi(str.substr(i, 1)) * pow(2, str.length() - i - 1);
	}
	
	return val;
}

// returns true if register was written to during previous CPU cycle
bool CPU::registerWasTouched()
{
    return touched_register.first;
}

// returns current clock cycle count
int CPU::getTotalClockCycles()
{
    return total_clock_cycles;
}

// returns current PC value
int CPU::getPC() 
{
    return pc;
}

void CPU::resetTouchedRegister()
{
    touched_register.first = false;
}

// returns true if memory was written to during previous CPU cycle
bool CPU::memoryWasTouched()
{
    return touched_memory.first;
}

void CPU::resetTouchedMemory()
{
    touched_memory.first = false;
}

// takes register 0-31 and an integer value to store
void CPU::updateRegisterFile(int i, int val)
{
    registerfile[i] = val;
}

// takes takes memory address / 4 and an integer value to store
void CPU::updateMemory(int i, int val)
{
    d_mem[i] = val;
}

// outputs to console the most recently modified register and its new value
void CPU::printTouchedRegister()
{
    int i = touched_register.second;
    std::cout << registerfile_name[i] << " is modified to 0x" << std::hex << registerfile[i] << std::dec << std::endl;
}

// outputs to console the most recently modified memory address and its new value
void CPU::printTouchedMemory()
{
    int i = touched_memory.second;
    std::cout << "memory 0x" << std::hex << d_mem_address[i] << " is modified to 0x" << d_mem[i] << std::dec << std::endl;
}

int CPU::fourMostSigFromPC()
{
    std::bitset<32> bt(pc);
    std::string str = bt.to_string();
    str = str.substr(0, 4);
    str += "0000000000000000000000000000";
    return convertBinaryStrToInt(str);
}

#endif