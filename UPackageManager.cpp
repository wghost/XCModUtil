#include "UPackageManager.h"

std::map<std::string, UPackage> UPackageManager::PackagesMap;

void UPackageManager::RegisterPackage(const UPackage& package)
{
    if (PackagesMap[package.PackageName].ReaderPtr == nullptr)
    {
        PackagesMap[package.PackageName] = package;
    }
}

void UPackageManager::UnregisterPackage(const std::string name)
{
    if (PackagesMap.count(name) > 0)
    {
        PackagesMap.erase(name);
    }
}

const UPackage& UPackageManager::FindPackage(const std::string name)
{
    return PackagesMap[name];
}
