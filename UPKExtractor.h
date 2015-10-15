#ifndef UPKEXTRACTOR_H
#define UPKEXTRACTOR_H

#include "UPKReader.h"

namespace UPKExtractor
{
    void ExtractPackageHeader(UPKReader* package, std::string fileName);
    void ExtractPackageObjects(UPKReader* package, std::string outDir, std::string nameMask = "");
    void ExtractEntry(UPKReader* package, UObjectReference objRef, std::string fileName);
    void ExtractEntry(UPKReader* package, std::string fullName, std::string fileName);

    void ComparePackages(UPKReader &package1, UPKReader &package2, std::string fileName);
    bool CompareExportTableEntries(FObjectExport entry1, FObjectExport entry2, bool IgnoreOffsets = true);

    std::string CreatePath(std::string fullName, std::string dirName);
    std::string MySenderName();
};

#endif // UPKEXTRACTOR_H
