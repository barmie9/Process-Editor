#include "ProcessEditor.h"
#include <tlhelp32.h>
#include <iostream>
#include <psapi.h>


ProcessEditor::ProcessEditor(const std::string& processName)
    : processName(processName), hProcess(NULL), processID(0) {}

ProcessEditor::~ProcessEditor() {
    CloseProcess();
}

bool ProcessEditor::OpenProcess() {
    processID = GetProcessIDByName(processName);
    if (processID == 0) {
        std::cerr << "Process not found!" << std::endl;
        return false;
    }

    hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (hProcess == NULL) {
        std::cerr << "Failed to open process!" << std::endl;
        return false;
    }

    return true;
}

void ProcessEditor::CloseProcess() {
    if (hProcess != NULL) {
        CloseHandle(hProcess);
        hProcess = NULL;
    }
}

bool ProcessEditor::ReadMemory(LPVOID address, LPVOID buffer, SIZE_T size, bool isTarget32Bit) {
    return ReadProcessMemory(hProcess, address, buffer, size, NULL) != 0;
}

bool ProcessEditor::WriteMemory(LPVOID address, LPVOID buffer, SIZE_T size, bool isTarget32Bit) {
    return WriteProcessMemory(hProcess, address, buffer, size, NULL) != 0;
}

LPVOID ProcessEditor::AllocateMemory(SIZE_T size, DWORD protection) {
    return VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT | MEM_RESERVE, protection);
}

bool ProcessEditor::FreeMemory(LPVOID address) {
    return VirtualFreeEx(hProcess, address, 0, MEM_RELEASE) != 0;
}

std::vector<BYTE> ProcessEditor::GenerateJumpCode(LPVOID from, LPVOID to, bool isTarget32Bit) {
    std::vector<BYTE> jumpCode;

    if (isTarget32Bit) {
        // JMP rel32 dla 32-bit
        jumpCode.push_back(0xE9); // Opcode dla JMP
        DWORD offset = (DWORD)((DWORD_PTR)to - ((DWORD_PTR)from + 5)); // Oblicz offset
        jumpCode.insert(jumpCode.end(), reinterpret_cast<BYTE*>(&offset), reinterpret_cast<BYTE*>(&offset) + 4);
    } else {
        // JMP [RIP+0] dla 64-bit
        jumpCode = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 }; // JMP [RIP+0]
        // Dodaj 64-bitowy adres docelowy
        DWORD_PTR targetAddress = (DWORD_PTR)to;
        jumpCode.insert(jumpCode.end(), reinterpret_cast<BYTE*>(&targetAddress), reinterpret_cast<BYTE*>(&targetAddress) + 8);
    }

    return jumpCode;
}

bool ProcessEditor::InjectCode(LPVOID address, const std::vector<BYTE>& code, bool isTarget32Bit) {
    // Rozmiar oryginalnego kodu, który zostanie zast¹piony przez jumpCode
    size_t jumpCodeSize = isTarget32Bit ? 5 : 14; // 5 bajtów dla 32-bit, 14 bajtów dla 64-bit

    // Alokuj pamiêæ dla nowego kodu (oryginalny kod + nowy kod + skok powrotny)
    LPVOID newMemory = AllocateMemory(code.size() + jumpCodeSize + 32, PAGE_EXECUTE_READWRITE); // Dodatkowe miejsce na bezpieczeñstwo
    if (!newMemory) {
        std::cerr << "Failed to allocate memory!" << std::endl;
        return false;
    }

    // Zapisz nowy kod w zaalokowanej pamiêci
    if (!WriteMemory(newMemory, (LPVOID)code.data(), code.size(), isTarget32Bit)) {
        std::cerr << "Failed to write new code!" << std::endl;
        FreeMemory(newMemory);
        return false;
    }

    // Odczytaj oryginalny kod, który zostanie zast¹piony przez jumpCode
    std::vector<BYTE> originalCode(jumpCodeSize);
    if (!ReadMemory(address, originalCode.data(), originalCode.size(), isTarget32Bit)) {
        std::cerr << "Failed to read original code!" << std::endl;
        FreeMemory(newMemory);
        return false;
    }

    // Zapisz oryginalny kod w mapie
    originalCodeMap[address] = originalCode;

    // Dodaj oryginalny kod do nowej pamiêci (po nowym kodzie)
    LPVOID originalCodeInNewMemory = (BYTE*)newMemory + code.size();
    if (!WriteMemory(originalCodeInNewMemory, originalCode.data(), originalCode.size(), isTarget32Bit)) {
        std::cerr << "Failed to write original code to new memory!" << std::endl;
        FreeMemory(newMemory);
        return false;
    }

    // Dodaj skok powrotny do miejsca tu¿ po oryginalnym kodzie
    LPVOID returnAddress = (BYTE*)address + jumpCodeSize;
    std::vector<BYTE> returnJumpCode = GenerateJumpCode((BYTE*)newMemory + code.size() + originalCode.size(), returnAddress, isTarget32Bit);
    if (!WriteMemory((BYTE*)newMemory + code.size() + originalCode.size(), returnJumpCode.data(), returnJumpCode.size(), isTarget32Bit)) {
        std::cerr << "Failed to write return jump code!" << std::endl;
        FreeMemory(newMemory);
        return false;
    }

    // Generuj kod do skoku do nowej pamiêci
    std::vector<BYTE> jumpCode = GenerateJumpCode(address, newMemory, isTarget32Bit);

    // Wstrzyknij skok do nowej pamiêci
    if (!WriteMemory(address, jumpCode.data(), jumpCode.size(), isTarget32Bit)) {
        std::cerr << "Failed to inject jump code!" << std::endl;
        FreeMemory(newMemory);
        return false;
    }

    // Zapisz zaalokowan¹ pamiêæ w mapie
    allocatedMemoryMap[address] = newMemory;

    return true;
}

bool ProcessEditor::RestoreCode(LPVOID address, bool isTarget32Bit) {
    // SprawdŸ, czy mamy oryginalny kod dla tego adresu
    if (originalCodeMap.find(address) == originalCodeMap.end()) {
        std::cerr << "No original code found for address: " << address << std::endl;
        return false;
    }

    // Przywróæ oryginalny kod
    if (!WriteMemory(address, originalCodeMap[address].data(), originalCodeMap[address].size(), isTarget32Bit)) {
        std::cerr << "Failed to restore original code!" << std::endl;
        return false;
    }

    // Zwolnij zaalokowan¹ pamiêæ
    if (allocatedMemoryMap.find(address) != allocatedMemoryMap.end()) {
        FreeMemory(allocatedMemoryMap[address]);
        allocatedMemoryMap.erase(address);
    }

    // Usuñ oryginalny kod z mapy
    originalCodeMap.erase(address);

    return true;
}

DWORD ProcessEditor::GetProcessIDByName(const std::string& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (processName == pe.szExeFile) {
                CloseHandle(hSnapshot);
                return pe.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return 0;
}

bool ProcessEditor::IsTarget32Bit() {
    BOOL isWow64 = FALSE;
    if (!IsWow64Process(hProcess, &isWow64)) {
        std::cerr << "Failed to check process architecture!" << std::endl;
        return false;
    }
    return isWow64;
}

LPVOID ProcessEditor::ResolvePointerChain(const std::vector<DWORD_PTR>& offsets, bool isTarget32Bit) {
    if (offsets.empty()) {
        return nullptr;
    }

    LPVOID currentAddress = reinterpret_cast<LPVOID>(offsets[0]);

    for (size_t i = 1; i < offsets.size(); ++i) {
        DWORD_PTR nextAddress;
        if (!ReadMemory(currentAddress, &nextAddress, sizeof(nextAddress), isTarget32Bit)) {
            std::cerr << "Failed to read memory at address: " << currentAddress << std::endl;
            return nullptr;
        }

        currentAddress = reinterpret_cast<LPVOID>(nextAddress + offsets[i]);
    }

    return currentAddress;
}

LPVOID ProcessEditor::GetModuleBaseAddress(const std::string& moduleName) {
    if (hProcess == NULL) {
        std::cerr << "Process is not open!" << std::endl;
        return nullptr;
    }

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processID);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create module snapshot!" << std::endl;
        return nullptr;
    }

    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(hSnapshot, &moduleEntry)) {
        do {
            if (moduleName == moduleEntry.szModule) {
                CloseHandle(hSnapshot);
                return moduleEntry.modBaseAddr;
            }
        } while (Module32Next(hSnapshot, &moduleEntry));
    }

    CloseHandle(hSnapshot);
    return nullptr; // Moduł nie został znaleziony
}
