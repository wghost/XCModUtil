#include "UPKExtractor.h"

#include <wx/string.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/regex.h>

#include "UObject.h"

std::string UPKExtractor::MySenderName()
{
    return "UPKExtractor";
}

void UPKExtractor::ExtractEntry(UPKReader* package, std::string fullName, std::string fileName)
{
    if (package == nullptr)
    {
        LogWarn("Package is not initialized!");
        return;
    }
    UObjectReference objRef = package->FindObject(fullName, false);
    if (objRef == 0)
    {
        LogError("Cannot find object entry by name: " + fullName);
        return;
    }
    ExtractEntry(package, objRef, fileName);
}

void UPKExtractor::ExtractEntry(UPKReader* package, UObjectReference objRef, std::string fileName)
{
    if (package == nullptr)
    {
        LogWarn("Package is not initialized!");
        return;
    }
    std::ofstream out(fileName);
    if (objRef == 0)
    {
        LogError("Null object reference in ExtractEntry!");
    }
    else if (objRef < 0)
    {
        out << package->FormatImport(-objRef, true) << std::endl;
    }
    else
    {
        out << package->FormatExport(objRef, true) << std::endl
            << "Attempting deserialization:" << std::endl
            << package->GetExportObject(objRef, true, false)->GetText() << std::endl;
    }
    out.close();
}

void UPKExtractor::ExtractPackageHeader(UPKReader* package, std::string fileName)
{
    if (package == nullptr)
    {
        LogWarn("Package is not initialized!");
        return;
    }
    std::ofstream out(fileName);
    out << package->FormatSummary() << std::endl
        << package->FormatNames(false) << std::endl
        << package->FormatImports(false) << std::endl
        << package->FormatExports(false);
    out.close();
}

std::string UPKExtractor::CreatePath(std::string fullName, std::string dirName)
{
    if (!wxDirExists(dirName))
        wxMkdir(dirName);
    std::string str = fullName;
    std::vector<std::string> names;
    unsigned pos = str.find('.');
    if (pos == std::string::npos)
    {
        return wxFileName(dirName + "/" + str + ".txt").GetFullPath().ToStdString();
    }
    while (pos != std::string::npos)
    {
        names.push_back(str.substr(0, pos));
        str = str.substr(pos + 1);
        pos = str.find('.');
    }
    std::string ret = str;
    str = dirName;
    for (unsigned i = 0; i < names.size(); ++i)
    {
        str = wxFileName(str + "/" + names[i]).GetFullPath().ToStdString();
        if (!wxDirExists(str))
            wxMkdir(str);
    }
    return wxFileName(str + "/" + ret + ".txt").GetFullPath().ToStdString();
}

void UPKExtractor::ExtractPackageObjects(UPKReader* package, std::string OutDir, std::string NameMask)
{
    if (package == nullptr)
    {
        LogWarn("Package is not initialized!");
        return;
    }
    wxRegEx mask(NameMask);
    if (!mask.IsValid())
    {
        LogError("Invalid regular expression in ExtractPackageObjects!");
        return;
    }
    std::string listPath = OutDir + ".txt";
    ExtractPackageHeader(package, listPath);
    std::vector<FObjectExport> exportTable = package->GetExportTable();
    for (unsigned i = 1; i < exportTable.size(); ++i)
    {
        if (!mask.Matches(exportTable[i].FullName))
        {
            continue;
        }
        LogDebug(exportTable[i].FullName);
        std::string filePath = CreatePath(exportTable[i].FullName, OutDir);
        std::ofstream out(filePath);
        out << package->FormatExport(i, true) << std::endl
            << package->GetExportObject(i, true)->GetText() << std::endl;
        out.close();
    }
}

void UPKExtractor::ComparePackages(UPKReader &package1, UPKReader &package2, std::string fileName)
{
    std::ofstream out(fileName);
    if (package1.GetSummary().Version != package2.GetSummary().Version)
    {
        out << package1.GetPackageName() << " version: " << package1.GetSummary().Version
            << "\t" << package2.GetPackageName() << " version: " << package2.GetSummary().Version << std::endl;
    }
    if (package1.GetSummary().NameCount != package2.GetSummary().NameCount)
    {
        out << package1.GetPackageName() << " NameCount: " << package1.GetSummary().NameCount
            << "\t" << package2.GetPackageName() << " NameCount: " << package2.GetSummary().NameCount << std::endl;
    }
    if (package1.GetSummary().ExportCount != package2.GetSummary().ExportCount)
    {
        out << package1.GetPackageName() << " ExportCount: " << package1.GetSummary().ExportCount
            << "\t" << package2.GetPackageName() << " ExportCount: " << package2.GetSummary().ExportCount << std::endl;
    }
    if (package1.GetSummary().ImportCount != package2.GetSummary().ImportCount)
    {
        out << package1.GetPackageName() << " ImportCount: " << package1.GetSummary().ImportCount
            << "\t" << package2.GetPackageName() << " ImportCount: " << package2.GetSummary().ImportCount << std::endl;
    }
    out << "Comparing " << package1.GetPackageName() << " name table entries to " << package2.GetPackageName() << " name table entries.\n";
    unsigned NamesNotFound = 0;
    for (unsigned i = 0; i < package1.GetSummary().NameCount; ++i)
    {
        int foundIdx = package2.FindName(package1.GetNameEntry(i).Name);
        if (foundIdx < 0)
        {
            out << package1.GetNameEntry(i).Name << " (index = " << i << ") is not found.\n";
            ++NamesNotFound;
        }
    }
    if (NamesNotFound > 0)
    {
        out << "Number of names not found: " << NamesNotFound << std::endl;
    }
    out << "Comparing " << package1.GetPackageName() << " import table entries to " << package2.GetPackageName() << " import table entries.\n";
    unsigned ImportsNotFound = 0, ImportsDifferent = 0;
    for (unsigned i = 1; i <= package1.GetSummary().ImportCount; ++i)
    {
        UObjectReference foundIdx = package2.FindObjectMatchType(package1.GetEntryFullName(-i), package1.GetEntryType(-i), false);
        if (foundIdx >= 0)
        {
            out << package1.GetEntryFullName(i) << " (index = " << -(int)i << ") is not found.\n";
            ++ImportsNotFound;
        }
        else
        {
            bool bDifferent = false;
            if ((int)-i != foundIdx)
            {
                out << package1.GetEntryFullName(-i) << " index " << -(int)i << " was changed to " << foundIdx << ".\n";
                bDifferent = true;
            }
            if (std::string(package1.GetObjectTableData(-i).data(), package1.GetObjectTableData(-i).size()) !=
                std::string(package2.GetObjectTableData(foundIdx).data(), package2.GetObjectTableData(foundIdx).size()))
            {
                out << package1.GetEntryFullName(-i) << " ImportTable data was changed\n.";
                bDifferent = true;
            }
            ImportsDifferent += bDifferent;
        }
    }
    if (ImportsNotFound > 0)
    {
        out << "Number of imports not found: " << ImportsNotFound << std::endl;
    }
    if (ImportsDifferent > 0)
    {
        out << "Number of different imports: " << ImportsDifferent << std::endl;
    }
    out << "Comparing " << package1.GetPackageName() << " exports to " << package2.GetPackageName() << " exports.\n";
    unsigned ExportsNotFound = 0, ExportsDifferent = 0;
    for (unsigned i = 1; i <= package1.GetSummary().ExportCount; ++i)
    {
        UObjectReference foundIdx = package2.FindObjectMatchType(package1.GetEntryFullName(i), package1.GetEntryType(i));
        if (foundIdx <= 0)
        {
            out << package1.GetEntryFullName(i) << " (index = " << i << ") is not found.\n";
            ++ExportsNotFound;
        }
        else
        {
            bool bDifferent = false;
            if ((int)i != foundIdx)
            {
                out << package1.GetEntryFullName(i) << " index " << i << " was changed to " << foundIdx << ".\n";
                bDifferent = true;
            }
            if (!CompareExportTableEntries(package1.GetExportEntry(i), package2.GetExportEntry(foundIdx)))
            {
                out << package1.GetEntryFullName(i) << " ExportTable data was changed.\n";
                bDifferent = true;
            }
            if (std::string(package1.GetExportData(i).data(), package1.GetExportData(i).size()) !=
                std::string(package2.GetExportData(foundIdx).data(), package2.GetExportData(foundIdx).size()))
            {
                out << package1.GetEntryFullName(i) << " serialized data was changed.\n";
                bDifferent = true;
            }
            ExportsDifferent += bDifferent;
        }
    }
    if (ExportsNotFound > 0)
    {
        out << "Number of exports not found: " << ExportsNotFound << std::endl;
    }
    if (ExportsDifferent > 0)
    {
        out << "Number of different exports: " << ExportsDifferent << std::endl;
    }
    out.close();
}

bool UPKExtractor::CompareExportTableEntries(FObjectExport entry1, FObjectExport entry2, bool IgnoreOffsets)
{
    bool res = true;
    if (
        entry1.Name != entry2.Name
        || entry1.FullName != entry1.FullName
        || entry1.Type != entry2.Type
        || entry1.ParentClassRef != entry2.ParentClassRef
        || entry1.ArchetypeRef != entry2.ArchetypeRef
        || entry1.ObjectFlagsH != entry2.ObjectFlagsH
        || entry1.ObjectFlagsL != entry2.ObjectFlagsL
        || entry1.ExportFlags != entry2.ExportFlags
        || entry1.SerialSize != entry2.SerialSize
        || entry1.EntrySize != entry2.EntrySize
    )
    {
        res = false;
    }
    if (!IgnoreOffsets && entry1.SerialOffset != entry2.SerialOffset)
    {
        res = false;
    }
    return res;
}
