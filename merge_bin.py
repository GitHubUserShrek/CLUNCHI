Import("env")


def _freq_to_esptool(value):
    """Convert framework flash frequency value to esptool format."""
    if not value:
        return "80m"
    s = str(value).strip().lower().replace("l", "")
    if s.endswith("m"):
        return s
    if s.isdigit():
        return f"{int(s) // 1000000}m"
    return s


def merge_firmware(source, target, env):
    import os
    import sys
    import shutil
    import subprocess
    import json

    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    pioenv = env.subst("$PIOENV")

    platform = env.PioPlatform()
    board = env.BoardConfig()

    # ----------------------------------------
    # Read board settings automatically
    # ----------------------------------------
    chip = str(board.get("build.mcu", "esp32c3")).lower()
    flash_mode = str(board.get("build.flash_mode", "dio"))
    flash_freq = _freq_to_esptool(board.get("build.f_flash", "80000000L"))
    flash_size = str(board.get(
        "upload.flash_size",
        board.get("build.flash_size", "4MB")
    ))
    app_offset = str(board.get("upload.offset_address", "0x10000"))

    # ----------------------------------------
    # Paths
    # ----------------------------------------
    esptool_dir = platform.get_package_dir("tool-esptoolpy")
    framework_dir = platform.get_package_dir("framework-arduinoespressif32")

    esptool_path = os.path.join(esptool_dir, "esptool.py")
    boot_app0_src = os.path.join(
        framework_dir, "tools", "partitions", "boot_app0.bin"
    )

    bootloader_src = os.path.join(build_dir, "bootloader.bin")
    partitions_src = os.path.join(build_dir, "partitions.bin")
    firmware_src = os.path.join(build_dir, "firmware.bin")

    # ----------------------------------------
    # Friendly names per board
    # ----------------------------------------
    names = {
        "lolin_c3_mini":       "CLUNCHI_C3_MINI_V1",
        "seeed_xiao_esp32c5":  "CLUNCHI_XIAO_C5_V1",
    }
    base_name = names.get(pioenv, pioenv)

    # Chip family for manifest
    chip_families = {
        "esp32c3": "ESP32-C3",
        "esp32c5": "ESP32-C5",
        "esp32c6": "ESP32-C6",
        "esp32s3": "ESP32-S3",
        "esp32":   "ESP32",
    }
    chip_family = chip_families.get(chip, chip.upper())

    # ----------------------------------------
    # Output directories
    # ----------------------------------------
    merged_dir = os.path.join(project_dir, "merged")
    board_dir = os.path.join(merged_dir, base_name)
    os.makedirs(board_dir, exist_ok=True)

    # ----------------------------------------
    # Validate inputs
    # ----------------------------------------
    missing = [f for f in [bootloader_src, partitions_src, firmware_src]
               if not os.path.exists(f)]
    if missing:
        for f in missing:
            print(f"ERROR: Missing file: {f}")
        return

    # ----------------------------------------
    # Copy individual files for web flasher
    # ----------------------------------------
    shutil.copy2(bootloader_src, os.path.join(board_dir, "bootloader.bin"))
    shutil.copy2(partitions_src, os.path.join(board_dir, "partitions.bin"))
    shutil.copy2(firmware_src, os.path.join(board_dir, "firmware.bin"))

    has_boot_app0 = os.path.exists(boot_app0_src)
    if has_boot_app0:
        shutil.copy2(boot_app0_src, os.path.join(board_dir, "boot_app0.bin"))

    # ----------------------------------------
    # Build merged factory image
    # ----------------------------------------
    factory_bin = os.path.join(board_dir, f"{base_name}_FACTORY.bin")

    cmd = [
        sys.executable, esptool_path,
        "--chip", chip,
        "merge_bin",
        "-o", factory_bin,
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
        "0x0000", bootloader_src,
        "0x8000", partitions_src,
    ]

    if has_boot_app0:
        cmd += ["0xE000", boot_app0_src]

    cmd += [app_offset, firmware_src]

    # ----------------------------------------
    # Generate manifest.json for web flasher
    # ----------------------------------------
    parts = [
        {"path": "bootloader.bin", "offset": 0},
        {"path": "partitions.bin", "offset": 32768},
    ]

    if has_boot_app0:
        parts.append({"path": "boot_app0.bin", "offset": 57344})

    parts.append({"path": "firmware.bin", "offset": int(app_offset, 16)})

    manifest = {
        "name": f"CLUNCHI ({base_name})",
        "builds": [
            {
                "chipFamily": chip_family,
                "parts": parts,
            }
        ]
    }

    manifest_path = os.path.join(board_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    # ----------------------------------------
    # Generate flash info text
    # ----------------------------------------
    info_path = os.path.join(board_dir, "FLASH_INFO.txt")
    with open(info_path, "w") as f:
        f.write(f"CLUNCHI Flash Info — {base_name}\n")
        f.write(f"{'=' * 48}\n\n")
        f.write(f"Chip:       {chip_family}\n")
        f.write(f"Flash mode: {flash_mode}\n")
        f.write(f"Flash freq: {flash_freq}\n")
        f.write(f"Flash size: {flash_size}\n\n")
        f.write(f"Option 1: Single factory image\n")
        f.write(f"  Flash {base_name}_FACTORY.bin at 0x0000\n\n")
        f.write(f"Option 2: Individual files\n")
        f.write(f"  bootloader.bin  -> 0x0000\n")
        f.write(f"  partitions.bin  -> 0x8000\n")
        if has_boot_app0:
            f.write(f"  boot_app0.bin   -> 0xE000\n")
        f.write(f"  firmware.bin    -> {app_offset}\n")

    # ----------------------------------------
    # Execute merge
    # ----------------------------------------
    print()
    print("=" * 48)
    print("  CLUNCHI — Building release package")
    print("=" * 48)
    print(f"  Environment : {pioenv}")
    print(f"  Chip        : {chip_family}")
    print(f"  Flash mode  : {flash_mode}")
    print(f"  Flash freq  : {flash_freq}")
    print(f"  Flash size  : {flash_size}")
    print(f"  App offset  : {app_offset}")
    print(f"  Output dir  : {board_dir}")
    print("=" * 48)
    print()

    try:
        subprocess.run(cmd, check=True)
        factory_size = os.path.getsize(factory_bin)
        firmware_size = os.path.getsize(firmware_src)

        print()
        print(f"  Output directory: {board_dir}/")
        print()
        print(f"  Files:")
        print(f"    bootloader.bin")
        print(f"    partitions.bin")
        if has_boot_app0:
            print(f"    boot_app0.bin")
        print(f"    firmware.bin            ({firmware_size // 1024} KB)")
        print(f"    {base_name}_FACTORY.bin ({factory_size // 1024} KB)")
        print(f"    manifest.json")
        print(f"    FLASH_INFO.txt")
        print()
        print(f"  Web flasher: use individual files at their offsets")
        print(f"  Desktop:     flash FACTORY.bin at 0x0000")
        print()
        print("=" * 48)
        print()
    except subprocess.CalledProcessError as e:
        print(f"\n  MERGE FAILED: {e}\n")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)