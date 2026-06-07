import ctypes
import os
import sys
import platform

def main():
    print("========================================")
    print("   MONOEYE RUNTIME VALIDATOR (v1.0)")
    print("========================================")
    print(f"OS: {platform.system()} {platform.release()}")
    print(f"Python: {sys.version}")
    print("----------------------------------------")

    dll_name = "XR_APILAYER_NOVENDOR_monoeye.dll"
    if platform.system() != "Windows":
        dll_name = "XR_APILAYER_NOVENDOR_monoeye.so"

    dll_path = os.path.abspath(dll_name)
    print(f"Target: {dll_path}")

    if not os.path.exists(dll_path):
        print(f"[!] ERROR: {dll_name} not found in the current directory.")
        return

    print(f"Attempting to load {dll_name}...")
    try:
        if platform.system() == "Windows":
            # Use LOAD_WITH_ALTERED_SEARCH_PATH to find dependencies in the same folder
            lib = ctypes.WinDLL(dll_path, use_last_error=True)
        else:
            lib = ctypes.CDLL(dll_path)
        
        print("[OK] DLL loaded successfully into memory.")
        
        # Check for exports
        print("Checking for required exports...")
        try:
            negotiate = lib.xrNegotiateLoaderApiLayerInterface
            print("[OK] Found export: xrNegotiateLoaderApiLayerInterface")
        except AttributeError:
            print("[!] ERROR: Could not find export 'xrNegotiateLoaderApiLayerInterface'")
            print("    This DLL is likely not a valid OpenXR API layer.")

    except Exception as e:
        print(f"[!] FAILED to load DLL.")
        print(f"    Error: {str(e)}")
        
        if platform.system() == "Windows":
            err_code = ctypes.get_last_error()
            print(f"    Windows Error Code: {err_code}")
            
            # Common Windows error codes
            if err_code == 126:
                print("    -> ERROR_MOD_NOT_FOUND: One of the DLL's dependencies (Vulkan, CRT, etc.) is missing!")
            elif err_code == 193:
                print("    -> ERROR_BAD_EXE_FORMAT: Architecture mismatch (32-bit vs 64-bit)!")
            elif err_code == 127:
                print("    -> ERROR_PROC_NOT_FOUND: An expected function in a dependency was not found.")

    print("\nNext Step: Check Documents/MonoEye/monoeye.log to see if DllMain was reached.")
    print("========================================")

if __name__ == "__main__":
    main()
