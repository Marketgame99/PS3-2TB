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

    // Create a mutable copy of the command line string (required by CreateProcessA)
    char cmd[512];
    strcpy_s(cmd, commandLine.c_str());

    // Spawn the process hidden
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE); // Wait for scetool to finish
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
    // 1. Read the file into memory
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) return false;

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;
    file.close();

    // 2. Scan for the hex pattern
    auto it = search(buffer.begin(), buffer.end(), pattern.begin(), pattern.end());
    if (it == buffer.end()) {
        cout << "[-] Pattern not found in " << filename << endl;
        return false;
    }

    // 3. Calculate exact memory location
    size_t foundOffset = distance(buffer.begin(), it);
    cout << "[+] Pattern found at offset: 0x" << hex << foundOffset << dec << endl;

    // 4. Inject the Patch (e.g., NOP 60 00 00 00)
    for (size_t i = 0; i < patch.size(); ++i) {
        buffer[foundOffset + patchOffset + i] = patch[i];
    }

    // 5. Write the patched file back to disk
    ofstream outfile(filename, ios::binary | ios::trunc);
    if (!outfile.is_open()) return false;
    outfile.write((char*)buffer.data(), buffer.size());
    outfile.close();

    cout << "[+] Successfully patched " << filename << "!" << endl;
    return true;
}

bool PatchString(const string& filename, const string& searchString, const string& replaceString) {
    // 1. Read the file into memory
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) return false;

    streamsize size = file.tellg();
    file.seekg(0, ios::beg);
    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) return false;
    file.close();

    // 2. Search for the Sony URL
    auto it = search(buffer.begin(), buffer.end(), searchString.begin(), searchString.end());
    if (it == buffer.end()) {
        cout << "[-] Update URL not found in " << filename << endl;
        return false;
    }

    size_t foundOffset = distance(buffer.begin(), it);
    cout << "[+] Update URL found at offset: 0x" << hex << foundOffset << dec << endl;

    // 3. Inject the replacement string
    for (size_t i = 0; i < replaceString.length(); ++i) {
        buffer[foundOffset + i] = replaceString[i];
    }

    // 4. Pad the rest of the original string length with NULL bytes (0x00)
    for (size_t i = replaceString.length(); i < searchString.length(); ++i) {
        buffer[foundOffset + i] = 0x00;
    }

    // 5. Write back to disk
    ofstream outfile(filename, ios::binary | ios::trunc);
    if (!outfile.is_open()) return false;
    outfile.write(buffer.data(), buffer.size());
    outfile.close();

    cout << "[+] Successfully neutralized update URL in " << filename << "!" << endl;
    return true;
}

int main() {
    cout << "========================================" << endl;
    cout << "  PS3 2TB+ Patcher v1.0" << endl;
    cout << "========================================" << endl;

    // The Sony update server URL (You should verify this exact string in a hex editor)
    string sonyURL = "http://fus01.ps3.update.playstation.net";

    // The dummy URL to trap the update check
    string dummyURL = "http://127.0.0.1";

    cout << "\n[*] Decrypting update_plugin.sprx..." << endl;
    RunProcessSilent("scetool.exe -d update_plugin.sprx update_plugin.elf");

    cout << "[*] Patching auto-update URLs..." << endl;
    PatchString("update_plugin.elf", sonyURL, dummyURL);
    // You should also run this against vsh.elf just to be safe!

    //PatchString("vsh.elf", sonyURL, dummyURL);

    cout << "[*] Re-encrypting update_plugin.sprx..." << endl;
    RunProcessSilent("scetool.exe -e --type=SPRX --crypt-type=APP update_plugin.elf update_plugin_patched.sprx");
    // NOP Instruction in PowerPC (60 00 00 00)
    vector<uint8_t> nopPatch = { 0x60, 0x00, 0x00, 0x00 };

    /*
       NOTE: You must verify these byte patterns in IDA Pro for your specific firmware.
       These are example representations of the assembly described in the documentation.
    */

    // Pattern 1: LV2 Kernel Truncation (clrldi r28, r28, 32 -> add r0, r0, r9)
    vector<uint8_t> lv2_pattern = { 0x78, 0x0B, 0x00, 0x20,  // clrldi r0, r11, 32
                                    0x7B, 0x9C, 0x00, 0x20,  // clrldi r28, r28, 32 (Target)
                                    0x7C, 0x00, 0x4A, 0x14 }; // add r0, r0, r9

    // Pattern 2: emer_init 1.5TB Gate (cmpld cr7, r4, r0 -> bgt <fail>)
    vector<uint8_t> emer_pattern = { 0x7F, 0x84, 0x00, 0x40,  // cmpld cr7, r4, r0
                                     0x41, 0x9D, 0x00, 0x14 }; // bgt <fail> (Target)

        /*
      Pattern 3: emer_init UFS2 minfree reservation (8% -> 1%)
      Note: This is a placeholder pattern. You MUST verify the exact
      surrounding instructions in IDA Pro inside the newfs_cfs routine.
    */
    vector<uint8_t> minfree_pattern = { 0x38, 0x60, 0x00, 0x08,  // li r3, 8 (Target)
                                        0x90, 0x7F, 0x01, 0x20,  // stw r3, 0x120(r31) [Store into superblock]
                                        0x38, 0x80, 0x08, 0x00 }; // li r4, 2048 

    // The patch changes '8' to '1'
    vector<uint8_t> minfreePatch = { 0x38, 0x60, 0x00, 0x01 }; // li r3, 1

    // --- STEP 1: DECRYPT ---
    cout << "\n[*] Decrypting lv2_kernel.self..." << endl;
    if (!RunProcessSilent("scetool.exe -d lv2_kernel.self lv2_kernel.elf")) {
        cout << "[-] Failed to decrypt LV2! Is scetool missing?" << endl; return 1;
    }

    cout << "[*] Decrypting emer_init.self..." << endl;
    if (!RunProcessSilent("scetool.exe -d emer_init.self emer_init.elf")) {
        cout << "[-] Failed to decrypt emer_init!" << endl; return 1;
    }

    // --- STEP 2: PATCH ---
    cout << "\n[*] Scanning and Patching LV2 Kernel..." << endl;
    // Patch offset is 4 bytes in (skipping the first instruction to hit the target)
    if (!PatchBinary("lv2_kernel.elf", lv2_pattern, nopPatch, 4)) {
        cout << "[-] LV2 Patch failed!" << endl; return 1;
    }

    cout << "[*] Scanning and Patching emer_init..." << endl;
    // Patch offset is 4 bytes in (skipping the cmpld instruction to hit the bgt)
    if (!PatchBinary("emer_init.elf", emer_pattern, nopPatch, 4)) {
        cout << "[-] emer_init Patch failed!" << endl; return 1;
    }

    cout << "[*] Scanning and Patching emer_init minfree (8% -> 1%)..." << endl;
    // Patch offset is 0 bytes (we are overwriting the very first instruction in the pattern)
    if (!PatchBinary("emer_init.elf", minfree_pattern, minfreePatch, 0)) {
        cout << "[-] emer_init minfree Patch failed!" << endl; return 1;
    }
    // --- STEP 3: RE-ENCRYPT ---
    cout << "\n[*] Re-encrypting lv2_kernel.self..." << endl;
    // Note: scetool requires specific flags to re-encrypt CoreOS files properly.
    string lv2_encrypt = "scetool.exe -e --type=SELF --crypt-type=OS --self-type=LV2 --compress-data=TRUE lv2_kernel.elf lv2_kernel_patched.self";
    if (RunProcessSilent(lv2_encrypt)) cout << "[+] LV2 Re-encrypted!" << endl;

    cout << "[*] Re-encrypting emer_init.self..." << endl;
    string emer_encrypt = "scetool.exe -e --type=SELF --crypt-type=APP --self-auth-id=1010000001000003 emer_init.elf emer_init_patched.self";
    if (RunProcessSilent(emer_encrypt)) cout << "[+] emer_init Re-encrypted!" << endl;

    // --- STEP 4: CLEANUP ---
    DeleteFileA("lv2_kernel.elf");
    DeleteFileA("emer_init.elf");

    cout << "\n[+] SUCCESS! Patched files are ready for MFW Builder." << endl;
    system("pause");
    return 0;
}