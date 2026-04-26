Import("env")

def merge_firmware(source, target, env):
    import subprocess
    import os
    import sys

    build_dir = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")
    platform = env.PioPlatform()

    # Find esptool
    esptool_path = os.path.join(
        platform.get_package_dir("tool-esptoolpy"), "esptool.py"
    )

    # Input files
    bootloader = os.path.join(build_dir, "bootloader.bin")
    partitions = os.path.join(build_dir, "partitions.bin")
    firmware   = os.path.join(build_dir, "firmware.bin")

    # Output file
    output = os.path.join(project_dir, "CLUNCHI_C3_BETA_V1.bin")

    # Check all files exist
    for f in [bootloader, partitions, firmware]:
        if not os.path.exists(f):
            print(f"ERROR: Missing file: {f}")
            return

    cmd = [
        sys.executable, esptool_path,
        "--chip", "esp32c3",
        "merge_bin",
        "-o", output,
        "--flash_mode", "dio",
        "--flash_freq", "80m",
        "--flash_size", "4MB",
        "0x0000",  bootloader,
        "0x8000",  partitions,
        "0x10000", firmware,
    ]

    print("\n========================================")
    print("  Merging firmware into single .bin...")
    print("========================================\n")

    try:
        subprocess.run(cmd, check=True)
        size = os.path.getsize(output)
        print(f"\n  SUCCESS: {output}")
        print(f"  Size: {size // 1024} KB")
        print(f"\n  Flash this at offset 0x0000")
        print("========================================\n")
    except subprocess.CalledProcessError as e:
        print(f"\n  MERGE FAILED: {e}")

env.AddPostAction("$BUILD_DIR/firmware.bin", merge_firmware)