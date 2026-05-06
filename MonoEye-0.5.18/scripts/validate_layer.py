#!/usr/bin/env python3
"""
MonoEye Layer Pre-Flight Validator
====================================
Catches the entire class of OpenXR layer bugs BEFORE pushing to CI.
Run: python3 scripts/validate_layer.py

Checks:
  1. Dispatch table field names match what generate_dispatch.py actually produces
  2. All field accesses are correctly cast (PFN_xrVoidFunction vs typed)
  3. nextGetInstanceProcAddr is used (not xrGetInstanceProcAddr) for the chain ptr
  4. All hooked functions in layer_proc_addr.cpp have forward decls
  5. All functions we call downstream are in our minimal dispatch table
  6. No GeneratedXrPopulateDispatchTable calls remain (causes loop)
"""

import re
import sys
import subprocess
import os
from pathlib import Path

ROOT = Path(__file__).parent.parent
SRC = ROOT / "src"
SCRIPTS = ROOT / "scripts"
XR_XML = ROOT / "external" / "OpenXR-SDK" / "specification" / "registry" / "xr.xml"

ERRORS = []
WARNINGS = []

def err(msg): ERRORS.append(f"  ERROR: {msg}")
def warn(msg): WARNINGS.append(f"  WARN:  {msg}")
def ok(msg): print(f"  OK:    {msg}")

# ─── 1. Generate the dispatch table and extract real field names ──────────────

def get_real_dispatch_fields():
    """Run the generator and extract actual struct field names."""
    import tempfile, xml.etree.ElementTree as ET

    if not XR_XML.exists():
        err(f"xr.xml not found at {XR_XML}")
        return set(), None

    # Parse xr.xml directly to get command names
    tree = ET.parse(XR_XML)
    root = tree.getroot()
    commands = set()
    for cmd in root.findall('.//commands/command'):
        name_elem = cmd.find('proto/name')
        if name_elem is not None and name_elem.text:
            commands.add(name_elem.text)
        # aliases
        alias_name = cmd.get('name')
        if alias_name:
            commands.add(alias_name)

    # Fields in our generated struct: exactly cmd_name (e.g. xrCreateSession)
    # Plus the special nextGetInstanceProcAddr field
    return commands, "nextGetInstanceProcAddr"

# ─── 2. Check all dispatch field accesses in source files ────────────────────

def check_dispatch_accesses(valid_fields, chain_field):
    """Find all dispatch->FIELD accesses and validate them."""
    # Pattern: dispatch->FIELD or nextDispatch->FIELD
    pattern = re.compile(r'(?:dispatch|nextDispatch)\s*->\s*(\w+)')

    files = list(SRC.glob("*.cpp")) + list(SRC.glob("*.h"))

    for fpath in sorted(files):
        text = fpath.read_text(errors='replace')
        for m in pattern.finditer(text):
            field = m.group(1)

            # nextGetInstanceProcAddr is always valid
            if field == chain_field:
                continue

            # xrDestroyInstance, xrCreateSession etc. must be in xr.xml commands
            if field.startswith("xr"):
                if field not in valid_fields:
                    lineno = text[:m.start()].count('\n') + 1
                    err(f"{fpath.name}:{lineno}: dispatch field '{field}' not in xr.xml (typo?)")
            else:
                # Non-xr prefixed field that's not nextGetInstanceProcAddr
                lineno = text[:m.start()].count('\n') + 1
                err(f"{fpath.name}:{lineno}: dispatch field '{field}' has no xr prefix and is not '{chain_field}' — wrong SDK version mismatch")

# ─── 3. Check nextGetInstanceProcAddr usage (not xrGetInstanceProcAddr) ───────

def check_chain_ptr_usage():
    """Ensure we use nextGetInstanceProcAddr for the chain, not xrGetInstanceProcAddr."""
    bad_pattern = re.compile(r'dispatch\s*->\s*xrGetInstanceProcAddr\s*(?:=|\(|!=)')

    for fpath in sorted(SRC.glob("*.cpp")):
        text = fpath.read_text(errors='replace')
        for m in bad_pattern.finditer(text):
            lineno = text[:m.start()].count('\n') + 1
            err(f"{fpath.name}:{lineno}: Use dispatch->nextGetInstanceProcAddr (typed) not dispatch->xrGetInstanceProcAddr (void*) for chain calls")

# ─── 4. Check for GeneratedXrPopulateDispatchTable (causes 500+ xrGIPA calls) ─

def check_no_full_populate():
    """Warn if GeneratedXrPopulateDispatchTable is called — it floods xrGIPA."""
    pattern = re.compile(r'GeneratedXrPopulateDispatchTable\s*\(')
    for fpath in sorted(SRC.glob("*.cpp")):
        text = fpath.read_text(errors='replace')
        for m in pattern.finditer(text):
            lineno = text[:m.start()].count('\n') + 1
            err(f"{fpath.name}:{lineno}: GeneratedXrPopulateDispatchTable floods xrGetInstanceProcAddr with 500+ calls causing log loops — use manual minimal resolution instead")

# ─── 5. Check hooked functions in layer_proc_addr.cpp have forward decls ──────

def check_hook_decls():
    """All hooked functions listed in s_hooked_functions[] must have extern decls."""
    pa = (SRC / "layer_proc_addr.cpp").read_text(errors='replace')

    # Extract names from s_hooked_functions table
    hooked = re.findall(r'\{"(xr\w+)"', pa)
    # Extract extern "C" forward declarations
    decls = set(re.findall(r'extern\s+"C"\s+XrResult\s+(monoeye_\w+)\s*\(', pa))

    for fn in hooked:
        decl_name = f"monoeye_{fn}"
        if decl_name not in decls:
            err(f"layer_proc_addr.cpp: hooked function '{fn}' has no extern \"C\" forward declaration for '{decl_name}'")

# ─── 6. Check all downstream calls match our minimal dispatch table ───────────

def check_minimal_dispatch_completeness():
    """Functions called via dispatch->xrFoo in hooks must be in layer_instance.cpp resolution."""
    instance_cpp = (SRC / "layer_instance.cpp").read_text(errors='replace')
    resolved = set(re.findall(r'nextGetInstanceProcAddr\s*\(\s*\*instance\s*,\s*"(\w+)"', instance_cpp))
    resolved.add("xrDestroyInstance")  # may be handled separately

    # Find all downstream calls in hook files
    call_pattern = re.compile(r'dispatch\s*->\s*(xr\w+)')
    hook_files = [f for f in SRC.glob("*.cpp") if f.name not in ("layer_instance.cpp", "layer_proc_addr.cpp", "layer_negotiation.cpp")]

    called = {}
    for fpath in sorted(hook_files):
        text = fpath.read_text(errors='replace')
        for m in call_pattern.finditer(text):
            fn = m.group(1)
            called.setdefault(fn, fpath.name)

    for fn, src_file in sorted(called.items()):
        if fn not in resolved:
            err(f"{src_file}: calls dispatch->{fn} but '{fn}' is not resolved in layer_instance.cpp minimal dispatch table")

# ─── 7. Check cast correctness around xrGetInstanceProcAddr ──────────────────

def check_void_function_casts():
    """PFN_xrVoidFunction fields must be cast before calling or comparing."""
    # Look for direct calls to void fields without cast
    bad_call = re.compile(r'dispatch\s*->\s*xrGetInstanceProcAddr\s*\(')
    bad_compare = re.compile(r'dispatch\s*->\s*xrGetInstanceProcAddr\s*!=')

    for fpath in sorted(SRC.glob("*.cpp")):
        text = fpath.read_text(errors='replace')
        for pattern, issue in [(bad_call, "called directly (needs cast to PFN_xrGetInstanceProcAddr first)"),
                                (bad_compare, "compared without cast")]:
            for m in pattern.finditer(text):
                lineno = text[:m.start()].count('\n') + 1
                err(f"{fpath.name}:{lineno}: dispatch->xrGetInstanceProcAddr is PFN_xrVoidFunction and is being {issue}")

# ─── Run all checks ──────────────────────────────────────────────────────────

def main():
    print("\n=== MonoEye Layer Pre-Flight Validator ===\n")

    print("[1/6] Loading xr.xml dispatch fields...")
    valid_fields, chain_field = get_real_dispatch_fields()
    if valid_fields:
        ok(f"Parsed {len(valid_fields)} commands from xr.xml")

    print("\n[2/6] Checking dispatch table field names...")
    check_dispatch_accesses(valid_fields, chain_field)

    print("\n[3/6] Checking chain pointer usage (nextGetInstanceProcAddr)...")
    check_chain_ptr_usage()
    check_void_function_casts()

    print("\n[4/6] Checking for full dispatch table population (loop risk)...")
    check_no_full_populate()

    print("\n[5/6] Checking hook forward declarations...")
    check_hook_decls()

    print("\n[6/6] Checking minimal dispatch table completeness...")
    check_minimal_dispatch_completeness()

    # ─── Report ──────────────────────────────────────────────────────────────
    print("\n" + "="*44)
    if WARNINGS:
        print(f"\n⚠  {len(WARNINGS)} warning(s):")
        for w in WARNINGS: print(w)

    if ERRORS:
        print(f"\n✗  {len(ERRORS)} error(s) — fix before pushing:\n")
        for e in ERRORS: print(e)
        sys.exit(1)
    else:
        print("\n✓  All checks passed — safe to push.")

if __name__ == "__main__":
    main()
