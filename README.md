# PS3 4TB Internal HDD Auto-Patcher

A C++ automation tool designed to modify core PlayStation 3 firmware files, permanently removing the hardware and software limitations that prevent the use of internal hard drives larger than 1.5 TB. 

By utilizing this tool and repacking a Custom Firmware (CFW) `.PUP`, you can natively install, format, and utilize a 4TB internal SATA hard drive on a PlayStation 3.

## ⚠️ Disclaimer
**USE AT YOUR OWN RISK.** Modifying `lv2_kernel.self` and `emer_init.self` carries a high risk of hard-bricking your console. It is highly recommended to test your generated `.PUP` via the "Boot from USB" Debug option on a DEX console, or ensure you have a hardware flasher (Teensy/Raspberry Pi Pico) installed before flashing CoreOS files to your NAND/NOR. 

*(Note: Testing this firmware in the RPCS3 emulator will result in false positives, as RPCS3 uses High-Level Emulation and bypasses the PS3 kernel and UFS2 block drivers).*

## ✨ Features / Patches Applied

This tool automatically unpacks, hex-patches, and re-encrypts the following bottlenecks:

1. **The 1.5 TB Recovery Gate (`emer_init.self` / `sys_init_osd.self`)**
   * *The Problem:* Safe Mode contains a hardcoded artificial gate that aborts formatting if a drive exceeds 1536 GiB.
   * *The Patch:* NOPs the `bgt` instruction, allowing the UI to accept 4TB drives.
2. **The 2 TB LV2 Truncation (`lv2_kernel.self`)**
   * *The Problem:* The PS3 hypervisor handles 64-bit math, but the GameOS kernel truncates the region size to 32 bits (`clrldi r28, r28, 32`), blinding the system to anything past 2.2 TB.
   * *The Patch:* NOPs the truncation instruction, leaving the kernel 64-bit clean.
3. **The 8% UFS2 Space Reservation (`emer_init.self`)**
   * *The Problem:* The PS3 reserves 8% of the drive (`minfree`) for root overhead. On a 4TB drive, this locks away over 300 GB of usable space.
   * *The Patch:* Alters the PowerPC `li r3, 8` instruction to `li r3, 1`, reducing reserved space to 1% and reclaiming ~280 GB.
4. **Auto-Update Neutralization (`update_plugin.sprx`)**
   * *The Problem:* Accidental Sony OTA updates will overwrite the patched kernel and corrupt the 4TB file system.
   * *The Patch:* Replaces Sony update server URLs with `127.0.0.1` loopbacks.

## 📋 Prerequisites

* **Windows OS** (The C++ tool utilizes the Win32 API for silent process execution).
* **scetool.exe** (Must be placed in the same directory as the patcher, along with the `keys` folder).
* **PS3 PUP Extractor** (To unpack your base Custom Firmware).
* **MFW Builder / PS3 PUP Builder** (To correctly hash and repack the CoreOS `.PUP`).
* A base Custom Firmware (e.g., Evilnat 4.90 or 4.91).

## 🚀 Usage Instructions

### Step 1: Extract Base Firmware
1. Use a PUP Extractor to unpack your chosen CFW `PS3UPDAT.PUP`.
2. Extract `update_ros.tar` to locate **`lv2_kernel.self`**.
3. Extract `update_files.tar` to locate **`emer_init.self`**, **`sys_init_osd.self`**, and **`update_plugin.sprx`**.

### Step 2: Run the Patcher
1. Place the extracted `.self` and `.sprx` files in the root directory of this tool.
2. Ensure `scetool.exe` and its `keys` directory are present.
3. Run `PS3_4TB_Patcher.exe`.
4. The tool will silently decrypt the files to `.elf`, scan for the required PowerPC assembly byte patterns, inject the patches, and re-encrypt them back to signed PS3 binaries.

### Step 3: Format Parameter Adjustment (Fragment Size)
To prevent the `libfs_utility.sprx` (fsck) RAM exhaustion crash on a 4TB drive, you must ensure the filesystem format utilizes an **8192-byte** fragment size instead of the default 4096-byte size. *(This can be patched in the `newfs_cfs` arguments within `emer_init` during Step 2).*

### Step 4: Repack and Flash
1. Take your newly generated `_patched.self` files and replace the originals inside the unpacked PUP directories.
2. Open **MFW Builder**.
3. Load the modified directory. MFW Builder will automatically recalculate the CoreOS cryptographic hashes.
4. Build the new `PS3UPDAT.PUP`.
5. Install the firmware to your PS3 via Safe Mode (Option 6: System Update).

## 🛑 Troubleshooting: "FATAL ERROR: Pattern Missing"

If the tool aborts and displays a **FATAL ERROR**, this is a built-in safety mechanism. It means the tool scanned the decrypted `.elf` file and could not find the exact assembly hex bytes required to inject the patch. 

The tool aborts automatically to prevent generating a corrupted kernel that would brick your console.

**Why did this happen?**
1. **Firmware Version Mismatch (Offset Shifting):** Every time Sony releases a new firmware (e.g., 4.90 vs 4.91), the C++ compiler injects new timestamps and version strings. This shifts the physical location of the code, and sometimes alters the surrounding registers used by the compiler.
2. **Already Patched:** You may be trying to patch a file that has already been modified.

**How to fix it:**
You must manually verify the new hex pattern for your specific firmware version:
1. Open the decrypted `lv2_kernel.elf` or `emer_init.elf` in a disassembler like **IDA Pro** or **Ghidra**.
2. Search for the assembly instructions documented in the source code.
3. Once found, copy the new Hex Opcode sequence.
4. Open the `main.cpp` source code of this tool, update the `vector<uint8_t>` hex arrays to match your firmware's specific pattern, and recompile the tool.
## 🛠️ Compiling the Source
Open `PS3 2TB+ Patcher.cpp` in Visual Studio or compile via MinGW:
```bash
g++ main.cpp -o PS3_4TB_Patcher.exe -static-libgcc -static-libstdc++
