#ifndef PROCESSEDITOR_H
#define PROCESSEDITOR_H

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>

class ProcessEditor {
public:
    ProcessEditor(const std::string& processName);
    ~ProcessEditor();

    bool OpenProcess();
    void CloseProcess();

    bool ReadMemory(LPVOID address, LPVOID buffer, SIZE_T size, bool isTarget32Bit);
    bool WriteMemory(LPVOID address, LPVOID buffer, SIZE_T size, bool isTarget32Bit);

    bool InjectCode(LPVOID address, const std::vector<BYTE>& code, bool isTarget32Bit);
    bool RestoreCode(LPVOID address, bool isTarget32Bit);

    bool IsTarget32Bit();

    // Metoda zwaraca nam dokłady adres na podstawie danych ze wskaźnika
    LPVOID ResolvePointerChain(const std::vector<DWORD_PTR>& offsets, bool isTarget32Bit);

    // Metoda pobiera adres bazowy modułu po nazwie
    LPVOID GetModuleBaseAddress(const std::string& moduleName);

private:
    std::string processName;
    HANDLE hProcess;
    DWORD processID;

    std::unordered_map<LPVOID, std::vector<BYTE>> originalCodeMap; // Przechowuje oryginalny kod
    std::unordered_map<LPVOID, LPVOID> allocatedMemoryMap; // Przechowuje zaalokowana pamiec

    DWORD GetProcessIDByName(const std::string& processName);
    std::vector<BYTE> GenerateJumpCode(LPVOID from, LPVOID to, bool isTarget32Bit);
    LPVOID AllocateMemory(SIZE_T size, DWORD protection);
    bool FreeMemory(LPVOID address);
};

#endif // PROCESSEDITOR_H
