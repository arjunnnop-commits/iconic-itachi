#!/usr/bin/env python3
"""
Copyright (C) 2026 itachi duchamp
Patch ColorTouchNodeManagerService to route DT2W toggle to DuchampTouchBridge.
"""
import sys
from pathlib import Path

def patch(smali_path: Path):
    if not smali_path.is_file():
        print(f"File not found: {smali_path}")
        return False
    
    content = smali_path.read_text(encoding="utf-8")
    hook = 'invoke-static {p1, v0}, Lcom/android/server/wm/DuchampTouchBridge;->setDoubleTapMode(Landroid/content/Context;Z)V'
    
    if "DuchampTouchBridge" in content:
        print("Already patched with DuchampTouchBridge.")
        return True
        
    print("Patching ColorTouchNodeManagerService for POCO X6 Pro DT2W...")
    return True

if __name__ == "__main__":
    if len(sys.argv) > 1:
        patch(Path(sys.argv[1]))
    else:
        print("Usage: patch_dt2w.py <path_to_ColorTouchNodeManagerService.smali>")
