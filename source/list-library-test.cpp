#include <iostream>

#ifdef __linux__
#include <fstream>
#include <string>
#include <sstream>
#include <set>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#endif

// Add libnode for testing if it is linked to it
#include "libnode.h"

void listLibraries() {
#ifdef __linux__
    // "/proc/self/maps" refers to the memory map of the current process
    std::string path = "/proc/self/maps";
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << path << std::endl;
        return;
    }

    std::set<std::string> libraries;
    std::string line;
    while (std::getline(file, line)) {
        // A shared library typically appears after the string 'lib' in the line
        if (line.find("lib") != std::string::npos) {
            std::istringstream iss(line);
            std::string addr, perms, offset, dev, inode, path;
            if (iss >> addr >> perms >> offset >> dev >> inode >> path) {
                libraries.insert(path);
            }
        }
    }

    // Print the libraries
    std::cout << "Libraries loaded by the current process:\n";
    for (const auto& lib : libraries) {
        std::cout << lib << std::endl;
    }
#endif

#ifdef __APPLE__
    // Get the number of loaded images (dynamic libraries)
    unsigned int imageCount = _dyld_image_count();

    // Iterate over all loaded images and print their paths
    std::cout << "Libraries loaded by the current process:\n";
    for (unsigned int i = 0; i < imageCount; ++i) {
        const char* imageName = _dyld_get_image_name(i);
        std::cout << imageName << std::endl;
    }
#endif

#ifdef WIN32
    HANDLE process = GetCurrentProcess();  // Get handle to the current process
    if (process == NULL) {
        std::cerr << "Error opening process: " << GetLastError() << std::endl;
        return;
    }

    HMODULE modules[1024];
    DWORD needed;
    if (EnumProcessModules(process, modules, sizeof(modules), &needed)) {
        for (unsigned int i = 0; i < needed / sizeof(HMODULE); i++) {
            TCHAR moduleName[MAX_PATH];
            if (GetModuleFileNameEx(process, modules[i], moduleName, sizeof(moduleName) / sizeof(TCHAR))) {
                std::wcout << moduleName << std::endl;
            }
        }
    }

    CloseHandle(process);
#endif
}

int main() {
    std::cout << string_function() << std::endl;
    listLibraries();
    return 0;
}
