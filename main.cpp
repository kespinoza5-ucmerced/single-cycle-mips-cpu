#include <iostream>
#include <string>
#include <vector>
#include "CPU.h"

int main()
{
    CPU cpu;
    std::string filename, str;

    std::vector<std::string> instruction_vector;

    std::cout << "Enter the program file name to run:" << std::endl;

    std::getline(std::cin, filename);

    if (filename == "sample_part1.txt")
    {
        cpu.updateRegisterFile(9, 32);
        cpu.updateRegisterFile(10, 5);
        cpu.updateRegisterFile(16, 112);

        cpu.updateMemory(28, 5);
        cpu.updateMemory(29, 16);
    }
    else if (filename == "sample_part2.txt")
    {
        cpu.updateRegisterFile(16, 32);
        cpu.updateRegisterFile(4, 5);
        cpu.updateRegisterFile(5, 2);
        cpu.updateRegisterFile(7, 10);
    }

    while(cpu.fetch(filename))
    {
        std::cout << "total_clock_cycles " << cpu.getTotalClockCycles() << " :" << std::endl;

        if (cpu.registerWasTouched())
        {
            cpu.printTouchedRegister();
        }
        if (cpu.memoryWasTouched())
        {
            cpu.printTouchedMemory();
        }

        std::cout << "pc is modified to 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
        std::cout << std::endl;
    }

    std::cout << "program terminated: " << std::endl;
    std::cout << "total execution time is " << cpu.getTotalClockCycles() << " cycles" << std::endl;
}