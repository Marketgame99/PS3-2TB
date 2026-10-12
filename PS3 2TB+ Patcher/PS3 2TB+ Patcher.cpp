#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

// --- Win32 API Wrapper to silently run scetool.exe ---
bool RunProcessSilent(const string& commandLine) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    char cmd[512];
    strcpy_s(cmd, commandLine.c_str());

    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

// --- Binary Pattern Scanner and Patcher ---
bool PatchBinary(const string& filename, const vector<uint8_t>& pattern, const vector<uint8_t>& patch, int patchOffset = 0) {
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) return false;

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;
    file.close();

    auto it = search(buffer.begin(), buffer.end(), pattern.begin(), pattern.end());

    // ERROR CHECK: Pattern not found!
    if (it == buffer.end()) {
        return false;
    }

    size_t foundOffset = distance(buffer.begin(), it);
    cout << "[+] Pattern found at offset: 0x" << hex << foundOffset << dec << endl;

    for (size_t i = 0; i < patch.size(); ++i) {
        buffer[foundOffset + patchOffset + i] = patch[i];
    }

    ofstream outfile(filename, ios::binary | ios::trunc);
    if (!outfile.is_open()) return false;
    outfile.write((char*)buffer.data(), buffer.size());
    outfile.close();

    return true;
}

// --- Fatal Error Handler ---
void FatalError(const string& message) {
    cout << "\n=======================================================" << endl;
    cout << "[!] FATAL ERROR: " << message << endl;
    cout << "[!] The exact byte pattern could not be located." << endl;
    cout << "[!] Reason 1: The firmware version offsets have shifted." << endl;
    cout << "[!] Reason 2: The file has already been patched." << endl;
    cout << "[!] ABORTING to prevent a console brick." << endl;
    cout << "=======================================================\n" << endl;

    // Clean up any decrypted leftover files before aborting
    DeleteFileA("lv2_kernel.elf");
    DeleteFileA("emer_init.elf");

    system("pause");
    exit(1);
}

int main() {
    cout << "========================================" << endl;
    cout << "  PS3 2TB+ Patcher v1.1" << endl;
    cout << "========================================" << endl;

    vector<uint8_t> nopPatch = { 0x60, 0x00, 0x00, 0x00 };
    vector<uint8_t> minfreePatch = { 0x38, 0x60, 0x00, 0x01 }; // li r3, 1

    // Patterns (Must be verified in IDA Pro per firmware version)
    vector<uint8_t> lv2_pattern = { 0x78, 0x0B, 0x00, 0x20, 0x7B, 0x9C, 0x00, 0x20, 0x7C, 0x00, 0x4A, 0x14 };
    vector<uint8_t> emer_pattern = { 0x7F, 0x84, 0x00, 0x40, 0x41, 0x9D, 0x00, 0x14 };
    vector<uint8_t> minfree_pattern = { 0x38, 0x60, 0x00, 0x08, 0x90, 0x7F, 0x01, 0x20, 0x38, 0x80, 0x08, 0x00 };

    // --- STEP 1: DECRYPT ---
    cout << "\n[*] Decrypting files..." << endl;
    if (!RunProcessSilent("scetool.exe -d lv2_kernel.self lv2_kernel.elf")) FatalError("Failed to decrypt LV2!");
    if (!RunProcessSilent("scetool.exe -d emer_init.self emer_init.elf")) FatalError("Failed to decrypt emer_init!");

    // --- STEP 2: PATCH WITH STRICT VERIFICATION ---
    cout << "\n[*] Scanning and Patching LV2 Kernel..." << endl;
    if (!PatchBinary("lv2_kernel.elf", lv2_pattern, nopPatch, 4)) {
        FatalError("LV2 Kernel 32-bit truncation pattern missing.");
    }

    cout << "[*] Scanning and Patching emer_init (1.5TB Gate)..." << endl;
    if (!PatchBinary("emer_init.elf", emer_pattern, nopPatch, 4)) {
        FatalError("emer_init 1.5TB gate pattern missing.");
    }

    cout << "[*] Scanning and Patching emer_init (8% -> 1% minfree)..." << endl;
    if (!PatchBinary("emer_init.elf", minfree_pattern, minfreePatch, 0)) {
        FatalError("emer_init minfree reservation pattern missing.");
    }

    // --- STEP 3: RE-ENCRYPT ---
    cout << "\n[*] Re-encrypting files..." << endl;
    RunProcessSilent("scetool.exe -e --type=SELF --crypt-type=OS --self-type=LV2 --compress-data=TRUE lv2_kernel.elf lv2_kernel_patched.self");
    RunProcessSilent("scetool.exe -e --type=SELF --crypt-type=APP --self-auth-id=1010000001000003 emer_init.elf emer_init_patched.self");

    // --- STEP 4: CLEANUP ---
    DeleteFileA("lv2_kernel.elf");
    DeleteFileA("emer_init.elf");

    cout << "\n[+] SUCCESS! Patched files are ready for MFW Builder." << endl;
    system("pause");
    return 0;
}