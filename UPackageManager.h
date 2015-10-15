#ifndef UPACKAGEMANAGER_H
#define UPACKAGEMANAGER_H

#include <string>
#include <map>
#include "UPKDeclarations.h"

#define _RegisterPackage(x)     UPackageManager::RegisterPackage(x)
#define _UnregisterPackage(x)   UPackageManager::UnregisterPackage(std::string(x))
#define _FindPackage(x)         UPackageManager::FindPackage(std::string(x))

struct UPackage
{
    std::string PackageName = "";
    std::string UPKName = "";
    UPKReader* ReaderPtr = nullptr;
    UPackage() {}
    UPackage(std::string name, std::string upk, UPKReader* reader): PackageName(name), UPKName(upk), ReaderPtr(reader) {}
};

class UPackageManager
{
public:
    static void RegisterPackage(const UPackage& package);
    static void UnregisterPackage(const std::string name);
    static const UPackage& FindPackage(const std::string name);
private:
    static std::map<std::string, UPackage> PackagesMap;
};

#endif // UPACKAGEMANAGER_H
