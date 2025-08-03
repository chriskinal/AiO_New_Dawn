#!/usr/bin/env python3
# Custom build script to move AgIsoStack code to flash memory

Import("env")

# Get build flags
build_flags = env.get("BUILD_FLAGS", [])

# Add flags to move code out of ITCM
if "-DENABLE_ISOBUS_VT" in build_flags:
    # Force all AgIsoStack code to flash memory
    env.Append(
        CXXFLAGS=[
            "-ffunction-sections",
            "-fdata-sections"
        ]
    )
    
    # Add linker flags to place AgIsoStack in flash
    env.Append(
        LINKFLAGS=[
            "-Wl,--section-start=.agisostack=0x60080000"
        ]
    )
    
    print("ISOBUS Build: Moving AgIsoStack code to flash memory")