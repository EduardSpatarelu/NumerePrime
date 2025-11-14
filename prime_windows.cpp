#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// Functie pentru verificarea numerelor prime
bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    int limit = static_cast<int>(std::sqrt(n));
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Codul pe care il ruleaza fiecare copil
int childProcess() {
    // Copilul citeste intervalele prin STD_INPUT si scrie rezultatele prin STD_OUTPUT
    HANDLE hIn  = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hIn == INVALID_HANDLE_VALUE || hOut == INVALID_HANDLE_VALUE) {
        return 1;
    }

    int start, end;
    DWORD bytesRead;

    // Citeste capetele intervalului
    if (!ReadFile(hIn, &start, sizeof(start), &bytesRead, nullptr) || bytesRead != sizeof(start)) {
        return 1;
    }
    if (!ReadFile(hIn, &end, sizeof(end), &bytesRead, nullptr) || bytesRead != sizeof(end)) {
        return 1;
    }

    // Calculeaza numerele prime si le trimite parintelui
    for (int x = start; x <= end; ++x) {
        if (isPrime(x)) {
            DWORD bytesWritten;
            WriteFile(hOut, &x, sizeof(x), &bytesWritten, nullptr);
        }
    }

    // Trimite 0 ca semnal de terminare
    int terminator = 0;
    DWORD bytesWritten;
    WriteFile(hOut, &terminator, sizeof(terminator), &bytesWritten, nullptr);

    return 0;
}

int main(int argc, char* argv[]) {
    // Daca acest proces este copil
    if (argc > 1 && std::string(argv[1]) == "child") {
        return childProcess();
    }

    // Codul procesului parinte
    const int N = 10000;
    const int NUM_PROCS = 10;
    const int INTERVAL = N / NUM_PROCS;

    struct ChildInfo {
        HANDLE hReadFromChild;  // pipe copil -> parinte
        HANDLE hWriteToChild;   // pipe parinte -> copil
        PROCESS_INFORMATION pi; // informatii proces copil
    };

    std::vector<ChildInfo> children(NUM_PROCS);

    // Setari pentru pipe-uri mostenibile de procese copil
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = TRUE;

    // Obtinem calea executabilului pentru a lansa copii
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);

    for (int i = 0; i < NUM_PROCS; ++i) {
        HANDLE p2cRead, p2cWrite; // pipe parinte -> copil
        HANDLE c2pRead, c2pWrite; // pipe copil -> parinte

        // Cream pipe-urile
        CreatePipe(&p2cRead, &p2cWrite, &sa, 0);
        CreatePipe(&c2pRead, &c2pWrite, &sa, 0);

        // Setam ce capete sunt mostenibile
        SetHandleInformation(p2cWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(c2pRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags |= STARTF_USESTDHANDLES;

        // Redirectionam STDIN si STDOUT ale copilului prin pipe-uri
        si.hStdInput  = p2cRead;
        si.hStdOutput = c2pWrite;
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        // Construim linia de comanda: "executabil.exe child"
        std::string cmd = std::string("\"") + exePath + "\" child";

        // CreateProcessA cere buffer modificabil -> il cream
        char* cmdLine = new char[cmd.size() + 1];
        strcpy(cmdLine, cmd.c_str());

        // Pornim procesul copil
        if (!CreateProcessA(
                nullptr,
                cmdLine,
                nullptr,
                nullptr,
                TRUE,   // permite mostenirea handle-urilor
                0,
                nullptr,
                nullptr,
                &si,
                &pi)) {
            std::cerr << "CreateProcess failed\n";
            delete[] cmdLine;
            return 1;
        }

        delete[] cmdLine;

        // Parintele nu foloseste aceste capete
        CloseHandle(p2cRead);
        CloseHandle(c2pWrite);

        children[i].hReadFromChild = c2pRead;
        children[i].hWriteToChild  = p2cWrite;
        children[i].pi = pi;

        // Trimitem intervalul copilului
        int start = i * INTERVAL + 1;
        int end   = (i + 1) * INTERVAL;

        if (i == NUM_PROCS - 1)
            end = N;

        DWORD bytesWritten;
        WriteFile(children[i].hWriteToChild, &start, sizeof(start), &bytesWritten, nullptr);
        WriteFile(children[i].hWriteToChild, &end,   sizeof(end),   &bytesWritten, nullptr);

        // Inchidem capatul de scriere catre copil (gata transmisia)
        CloseHandle(children[i].hWriteToChild);
        children[i].hWriteToChild = nullptr;
    }

    // Citim rezultatele de la fiecare copil
    std::cout << "Numere prime intre 1 si 10000:\n";

    for (int i = 0; i < NUM_PROCS; ++i) {
        int value;
        DWORD bytesRead;

        while (true) {
            if (!ReadFile(children[i].hReadFromChild, &value, sizeof(value), &bytesRead, nullptr))
                break;

            if (bytesRead == 0)
                break;

            if (value == 0)
                break;

            std::cout << value << " ";
        }

        CloseHandle(children[i].hReadFromChild);
    }

    std::cout << "\n";

    // Asteptam copiii sa termine
    for (int i = 0; i < NUM_PROCS; ++i) {
        WaitForSingleObject(children[i].pi.hProcess, INFINITE);
        CloseHandle(children[i].pi.hProcess);
        CloseHandle(children[i].pi.hThread);
    }

    return 0;
}
