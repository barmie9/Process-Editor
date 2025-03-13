#include "ProcessEditor.h"
#include <iostream>
#include "windows.h"

using namespace std;

int main() {
    string PROCESS_NAME = "Tutorial-x86_64.exe";
    //string PROCESS_NAME = "Tutorial-i386.exe";
    string DINPUT = "DINPUT8.dll";

    ProcessEditor editor(PROCESS_NAME);

    if (!editor.OpenProcess()) {
        std::cerr << "Failed to open process!" << std::endl;
        // Czekamy na użytkownika (np. aby zobaczyć efekty wstrzyknięcia)
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }

    // Sprawdź, czy proces jest 32-bitowy
    bool isTarget32Bit = editor.IsTarget32Bit();
    if(isTarget32Bit){
        std::cout << "PROCES 32 BITY" << std::endl;
    }
    else{
        std::cout << "PROCES 64 BITY" << std::endl;
    }

    Sleep(3000);

    // Pobierz base address głównego modułu (np. Tutorial-x86_64.exe)
    LPVOID baseAddress = editor.GetModuleBaseAddress(PROCESS_NAME);
    if (baseAddress == nullptr) {
        std::cerr << "Failed to get base address!" << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        return 1;
    }

    std::cout << "Base address: " << baseAddress << std::endl;

    // Przykładowy łańcuch wskaźników
    std::vector<DWORD_PTR> pointerChain = {
        reinterpret_cast<DWORD_PTR>(baseAddress) + 0x499ED//,  // baseAddress + offset
        //0x10,      // offset do pierwszego wskaŸnika
        //0x20       // offset do drugiego wskaŸnika
    };

    // Rozwiazanie łańcucha wskaźników
    LPVOID targetAddress = editor.ResolvePointerChain(pointerChain, isTarget32Bit);
    if (targetAddress == nullptr) {
        std::cerr << "Failed to resolve pointer chain!" << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        return 1;
    }



    LPVOID static_address = reinterpret_cast<LPVOID>(0x1000499ED); // Przykładowy adres
    //LPVOID static_address = reinterpret_cast<LPVOID>(0x429D8D); // Przykładowy adres

    // Przykład edycji wartoœci pod docelowym adresem
    //int newValue = 1000;
    //if (!editor.WriteMemory(static_address, &newValue, sizeof(newValue), isTarget32Bit)) {
    //    std::cerr << "Failed to write memory!" << std::endl;
    //}

    // Kod do wstrzyknięcia (dostosowany do architektury)
    std::vector<BYTE> newCode;

        // Kod do wstrzyknięcia
    if (isTarget32Bit) {
        // Kod dla 32-bit
        newCode = {
            //0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xFB, 0x99, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00
            0x90
        };
    } else {
        // Kod dla 64-bit
        newCode = {
            //0x90, // NOP (nic nie rób)
                // cmp [rbx+14], 01
            0x83, 0x7B, 0x14, 0x01,

            // je exit (skok o 5 bajty do przodu)
            0x74, 0x05,
            //0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xFB, 0x99, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00
        };
    }


    // Wstrzykiwanie kodu i przywracanie starego:
    string response;
    while(true){
        std::cin>> response;
        if(response == "inject"){
            // Wstrzykujemy kod
            if (editor.InjectCode(targetAddress, newCode, isTarget32Bit)) {
                std::cout << "Code injected successfully!" << std::endl;
            } else {
                std::cerr << "Failed to inject code!" << std::endl;
                std::cout << "Press Enter to exit..." << std::endl;
                return 1;
            }
        }
        else if(response == "restore"){
            // Przywracamy oryginalny kod
            if (editor.RestoreCode(targetAddress, isTarget32Bit)) {
                std::cout << "Original code restored successfully!" << std::endl;
            } else {
                std::cerr << "Failed to restore original code!" << std::endl;
                std::cout << "Press Enter to exit..." << std::endl;
                return 1;
            }
        }
        else if (response == "exit"){
            break;
        }
    }

    editor.CloseProcess();
    return 0;
}
