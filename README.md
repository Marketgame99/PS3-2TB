# PS3 2TB+ Internal HDD Auto-Patcher

A C++ automation tool designed to modify core PlayStation 3 firmware files, permanently removing the hardware and software limitations that prevent the use of internal hard drives larger than 1.5 TB. 

By utilizing this tool and repacking a Custom Firmware (CFW) `.PUP`, you can natively install, format, and utilize a 4TB internal SATA hard drive on a PlayStation 3.

## ⚠️ Disclaimer
**USE AT YOUR OWN RISK.** Modifying `lv2_kernel.self` and `emer_init.self` carries a high risk of hard-bricking your console. It is highly recommended to test your generated `.PUP` via the "Boot from USB" Debug option on a DEX console, or ensure you have a hardware flasher (Teensy/Raspberry Pi Pico) installed before flashing CoreOS files to your NAND/NOR. 

*(Note: Testing this firmware in the RPCS3 emulator will result in false positives, as RPCS3 uses High-Level Emulation and bypasses the PS3 kernel and UFS2 block drivers).*

## ✨ Features / Patches Applied

This tool automatically unpacks, hex-patches, and re-encrypts the following bottlenecks:

1. **The 1.5 TB Recovery Gate (`emer_init.self`)**
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
* **PS3 MFW Builder** (To correctly hash and repack the CoreOS `.PUP`).
* A base Custom Firmware (e.g., Evilnat 4.90 or 4.91).

## 🚀 Usage Instructions

### Step 1: Extract Base Firmware
1. Use a PUP Extractor to unpack your chosen CFW `PS3UPDAT.PUP`.
2. Extract `update_ros.tar` to locate **`lv2_kernel.self`**.
3. Extract `update_files.tar` to locate **`emer_init.self`** and **`update_plugin.sprx`**.

### Step 2: Run the Patcher
1. Place the extracted `.self` and `.sprx` files in the root directory of this tool.
2. Ensure `scetool.exe` and its `keys` directory are present.
3. Run `PS3_2TB_Patcher.exe`.
4. The tool will silently decrypt the files, scan for the required byte patterns, inject the patches, and output `_patched` versions of the files.

### Step 3: Prepare the Patched Files for Injection
Before using MFW Builder, you must rename the output files back to their original names and place them in a specific folder structure so MFW Builder knows exactly where to inject them.

1. Rename the files:
   * `lv2_kernel_patched.self` ➔ `lv2_kernel.self`
   * `emer_init_patched.self` ➔ `emer_init.self`
   * `update_plugin_patched.sprx` ➔ `update_plugin.sprx`
2. Create a new folder on your desktop called `Custom_PS3_Files`.
3. Inside that folder, create the following exact directory structure and place the files inside:
   * `Custom_PS3_Files/CoreOS/lv2_kernel.self`
   * `Custom_PS3_Files/dev_flash/sys/internal/emer_init.self`
   * `Custom_PS3_Files/dev_flash/vsh/module/update_plugin.sprx`

### Step 4: Build the Firmware using MFW Builder
You cannot just ZIP the files back up. You must use MFW Builder so the PS3's cryptographic CoreOS hashes are correctly recalculated.

1. Open **MFW Builder**.
2. In the **Original Firmware** field, select your unmodified base CFW `.PUP` (e.g., *Evilnat_4.91.PUP*).
3. In the **Modified Firmware** field, choose a save location and name for your new file (e.g., *Evilnat_4TB_Patched.PUP*).
4. On the left sidebar, **uncheck all default tasks** except for the following two:
   * `Patch CoreOS` (or `Change CoreOS files`)
   * `Patch custom files` (or `Replace dev_flash files`)
5. Click on the **Patch CoreOS** task in the sidebar, and point the directory setting to your `Custom_PS3_Files/CoreOS/` folder.
6. Click on the **Patch custom files** task in the sidebar, and point the directory setting to your `Custom_PS3_Files/dev_flash/` folder.
7. Click **Build MFW**.
8. MFW Builder will automatically unpack the base PUP, swap in your patched files, recalculate the secure hashes, and pack it into a pristine `.PUP`.

### Step 5: Format and Install
1. Put your newly built `PS3UPDAT.PUP` on a FAT32 USB drive (`PS3/UPDATE/PS3UPDAT.PUP`).
2. Insert your blank 4TB drive into the PS3.
3. Boot the PS3 into **Safe Mode**.
4. Select **Option 6: System Update**.
5. The PS3 will read your patched firmware, bypass the 1.5TB UI gate, format the drive with 1% reserved space, and boot into the XMB utilizing the full 64-bit geometry!

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
Open `main.cpp` in Visual Studio or compile via MinGW:
```bash
g++ main.cpp -o PS3_4TB_Patcher.exe -static-libgcc -static-libstdc++
