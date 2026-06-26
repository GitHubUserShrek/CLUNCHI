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
    import subprocess

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
    boot_app0 = os.path.join(
        framework_dir, "tools", "partitions", "boot_app0.bin"
    )

    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware = os.path.join(build_dir, "firmware.bin")

    # ----------------------------------------
    # Output — friendly names per board
    # ----------------------------------------
    output_dir = os.path.join(project_dir, "merged")
    os.makedirs(output_dir, exist_ok=True)

    names = {
        "lolin_c3_mini":       "CLUNCHI_C3_MINI_V1",
        "seeed_xiao_esp32c5":  "CLUNCHI_XIAO_C5_V1",
    }
    base_name = names.get(pioenv, pioenv)
    output = os.path.join(output_dir, f"{base_name}.bin")

    # ----------------------------------------
    # Validate inputs
    # ----------------------------------------
    missing = [f for f in [bootloader, partitions, firmware]
               if not os.path.exists(f)]
    if missing:
        for f in missing:
            print(f"ERROR: Missing file: {f}")
        return

    # ----------------------------------------
    # Build merge command
    # ----------------------------------------
    cmd = [
        sys.executable, esptool_path,
        "--chip", chip,
        "merge_bin",
        "-o", output,
        "--flash_mode", flash_mode,
        "--flash_freq", flash_freq,
        "--flash_size", flash_size,
        "0x0000", bootloader,
        "0x8000", partitions,
    ]

    if os.path.exists(boot_app0):
        cmd += ["0xE000", boot_app0]

    cmd += [app_offset, firmware]

    # ----------------------------------------
    # Execute
    # ----------------------------------------
    print()
    print("=" * 48)
    print("  CLUNCHI — Merging firmware")
    print("=" * 48)
    print(f"  Environment : {pioenv}")
    print(f"  Chip        : {chip}")
    print(f"  Flash mode  : {flash_mode}")
    print(f"  Flash freq  : {flash_freq}")
    print(f"  Flash size  : {flash_size}")
    print(f"  App offset  : {app_offset}")
    print(f"  Output      : {output}")
    print("=" * 48)
    print()

    try:
        subprocess.run(cmd, check=True)
        size = os.path.getsize(output)
        print()
        print(f"  SUCCESS: {output}")
        print(f"  Size:    {size // 1024} KB")
        print(f"  Flash at offset 0x0000")
        print()
        print("=" * 48)
        print()
    except subprocess.CalledProcessError as e:
        print(f"\n  MERGE FAILED: {e}\n")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)