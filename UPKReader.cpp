#include "UPKReader.h"

#include <cstring>
#include <cstdio>
#include <fstream>

#include "UPKLZOUtils.h"
#include "UObjectFactory.h"
#include "TextUtils.h"
#include "UPackageManager.h"
#include "LogService.h"

void UPKReader::LogErrorState(UPKReadErrors err)
{
    ReadError = err;
    LogError(FormatReadErrors(ReadError));
}

void UPKReader::ClearObjects()
{
    LogDebug("Clearing ObjectsMap.");
    for (std::map<uint32_t, UObject*>::iterator it = ObjectsMap.begin() ; it != ObjectsMap.end(); ++it)
    {
        delete it->second;
    }
    ObjectsMap.clear();
}

UPKReader::~UPKReader()
{
    ClearObjects();
    _UnregisterPackage(PackageName);
    LogDebug("Package " + PackageName + " unregistered.");
}

UPKReader::UPKReader(const char* filename)
{
    if (!LoadPackage(filename))
    {
        LogError("Object was not initialized properly due to read errors!");
    }
}

bool UPKReader::LoadPackage(const char* filename)
{
    if (filename != nullptr)
    {
        UPKFileName = filename;
        LogDebug("UPK File Name = " + UPKFileName);
        PackageName = GetFilenameNoExt(UPKFileName);
    }
    std::ifstream file(UPKFileName, std::ios::binary);
    if (!file)
    {
        LogErrorState(UPKReadErrors::FileError);
        return false;
    }
    UPKStream.str("");
    UPKStream << file.rdbuf();
    file.close();
    LogDebug("UPK file loaded into memory, reading package header...");
    if (!ReadPackageHeader())
        return false;
    if (_FindPackage(PackageName).PackageName == PackageName)
    {
        LogDebug("Duplicated package name: " + PackageName);
        PackageName += "_duplicate";
    }
    _RegisterPackage(UPackage(PackageName, GetFilename(UPKFileName), this));
    LogDebug("Package registered as " + PackageName);
    return true;
}

bool UPKReader::SavePackage(const char* filename)
{
    if (filename != nullptr)
    {
        UPKFileName = filename;
        LogDebug("UPK File Name = " + UPKFileName);
    }
    std::ofstream file(UPKFileName, std::ios::binary);
    if (!file)
    {
        LogErrorState(UPKReadErrors::FileError);
        return false;
    }
    file.write(UPKStream.str().data(), UPKStream.str().size());
    file.close();
    LogDebug("Package saved to " + UPKFileName);
    if (_FindPackage(PackageName).UPKName != GetFilename(UPKFileName))
    {
        _UnregisterPackage(PackageName);
        _RegisterPackage(UPackage(PackageName, GetFilename(UPKFileName), this));
        LogDebug("Package re-registered as " + PackageName);
    }
    return true;
}

bool UPKReader::ReinitializeHeader()
{
    if (!IsLoaded())
    {
        LogErrorState(UPKReadErrors::Uninitialized);
        return false;
    }
    return ReadPackageHeader();
}

bool UPKReader::Decompress()
{
    return DecompressLZOCompressedPackage(this);
}

bool UPKReader::ReadCompressedHeader()
{
    LogDebug("Reading compressed header...");
    ReadError = UPKReadErrors::NoErrors;
    Compressed = false;
    CompressedChunk = false;
    LastAccessedExportObjIdx = 0;
    UPKStream.seekg(0, std::ios::end);
    size_t Size = UPKStream.tellg();
    UPKStream.seekg(0);
    UPKStream.read(reinterpret_cast<char*>(&CompressedHeader.Signature), 4);
    if (CompressedHeader.Signature != 0x9E2A83C1)
    {
        LogErrorState(UPKReadErrors::BadSignature);
        return false;
    }
    UPKStream.read(reinterpret_cast<char*>(&CompressedHeader.BlockSize), 4);
    UPKStream.read(reinterpret_cast<char*>(&CompressedHeader.CompressedSize), 4);
    UPKStream.read(reinterpret_cast<char*>(&CompressedHeader.UncompressedSize), 4);
    CompressedHeader.NumBlocks = (CompressedHeader.UncompressedSize + CompressedHeader.BlockSize - 1) / CompressedHeader.BlockSize; // Gildor
    uint32_t CompHeadSize = 16 + CompressedHeader.NumBlocks * 8;
    Size -= CompHeadSize; /// actual compressed file size
    if (CompressedHeader.CompressedSize != Size ||
        CompressedHeader.UncompressedSize < Size ||
        CompressedHeader.UncompressedSize < CompressedHeader.CompressedSize)
    {
        LogErrorState(UPKReadErrors::BadVersion);
        return false;
    }
    CompressedHeader.Blocks.clear();
    for (unsigned i = 0; i < CompressedHeader.NumBlocks; ++i)
    {
        FCompressedChunkBlock Block;
        UPKStream.read(reinterpret_cast<char*>(&Block.CompressedSize), 4);
        UPKStream.read(reinterpret_cast<char*>(&Block.UncompressedSize), 4);
        CompressedHeader.Blocks.push_back(Block);
    }
    Compressed = true;
    CompressedChunk = true;
    LogDebug("Package is fully compressed, decompressing...");
    if (!Decompress())
    {
        LogErrorState(UPKReadErrors::IsCompressed);
        return false;
    }
    return true;
}

bool UPKReader::ReadPackageHeader()
{
    ClearObjects(); /// clear ObjectsMap
    CompressedHeader = FCompressedChunkHeader{};
    ReadError = UPKReadErrors::NoErrors;
    Compressed = false;
    CompressedChunk = false;
    LastAccessedExportObjIdx = 0;
    LogDebug("Reading package Summary...");
    UPKStream.seekg(0);
    UPKStream.read(reinterpret_cast<char*>(&Summary.Signature), 4);
    if (Summary.Signature != 0x9E2A83C1)
    {
        LogErrorState(UPKReadErrors::BadSignature);
        return false;
    }
    int32_t tmpVer;
    UPKStream.read(reinterpret_cast<char*>(&tmpVer), 4);
    Summary.Version = tmpVer % (1 << 16);
    Summary.LicenseeVersion = tmpVer >> 16;
    if (Summary.Version != 845)
    {
        LogDebug("Bad version, checking if package is fully compressed...");
        return ReadCompressedHeader();
    }
    Summary.HeaderSizeOffset = UPKStream.tellg();
    UPKStream.read(reinterpret_cast<char*>(&Summary.HeaderSize), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.FolderNameLength), 4);
    if (Summary.FolderNameLength > 0)
    {
        getline(UPKStream, Summary.FolderName, '\0');
    }
    else
    {
        Summary.FolderName = "";
    }
    UPKStream.read(reinterpret_cast<char*>(&Summary.PackageFlags), 4);
    Summary.NameCountOffset = UPKStream.tellg();
    UPKStream.read(reinterpret_cast<char*>(&Summary.NameCount), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.NameOffset), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.ExportCount), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.ExportOffset), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.ImportCount), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.ImportOffset), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.DependsOffset), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.SerialOffset), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.Unknown2), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.Unknown3), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.Unknown4), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.GUID), sizeof(Summary.GUID));
    UPKStream.read(reinterpret_cast<char*>(&Summary.GenerationsCount), 4);
    Summary.Generations.clear();
    for (unsigned i = 0; i < Summary.GenerationsCount; ++i)
    {
        FGenerationInfo EntryToRead;
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ExportCount), 4);
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameCount), 4);
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NetObjectCount), 4);
        Summary.Generations.push_back(EntryToRead);
    }
    UPKStream.read(reinterpret_cast<char*>(&Summary.EngineVersion), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.CookerVersion), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.CompressionFlags), 4);
    UPKStream.read(reinterpret_cast<char*>(&Summary.NumCompressedChunks), 4);
    Compressed = ((Summary.NumCompressedChunks > 0) || (Summary.CompressionFlags != 0));
    LogDebug("Reading CompressedChunks...");
    Summary.CompressedChunks.clear();
    for (unsigned i = 0; i < Summary.NumCompressedChunks; ++i)
    {
        FCompressedChunk CompressedChunk;
        UPKStream.read(reinterpret_cast<char*>(&CompressedChunk.UncompressedOffset), 4);
        UPKStream.read(reinterpret_cast<char*>(&CompressedChunk.UncompressedSize), 4);
        UPKStream.read(reinterpret_cast<char*>(&CompressedChunk.CompressedOffset), 4);
        UPKStream.read(reinterpret_cast<char*>(&CompressedChunk.CompressedSize), 4);
        Summary.CompressedChunks.push_back(CompressedChunk);
    }
    LogDebug("Reading UnknownDataChunk...");
    Summary.UnknownDataChunk.clear();
    /// for uncompressed packages unknown data is located between NumCompressedChunks and NameTable
    if (Summary.NumCompressedChunks < 1 && Summary.NameOffset - UPKStream.tellg() > 0)
    {
        Summary.UnknownDataChunk.resize(Summary.NameOffset - UPKStream.tellg());
    }
    /// for compressed packages unknown data is located between last CompressedChunk entry and first compressed data
    else if (Summary.NumCompressedChunks > 0 && Summary.CompressedChunks[0].CompressedOffset - UPKStream.tellg() > 0)
    {
        Summary.UnknownDataChunk.resize(Summary.CompressedChunks[0].CompressedOffset - UPKStream.tellg());
    }
    if (Summary.UnknownDataChunk.size() > 0)
    {
        UPKStream.read(Summary.UnknownDataChunk.data(), Summary.UnknownDataChunk.size());
    }
    if (Compressed == true)
    {
        LogDebug("Package is compressed, decompressing...");
        if (!Decompress())
        {
            LogErrorState(UPKReadErrors::IsCompressed);
            return false;
        }
        return true;
    }
    LogDebug("Reading NameTable...");
    NameTable.clear();
    UPKStream.seekg(Summary.NameOffset);
    for (unsigned i = 0; i < Summary.NameCount; ++i)
    {
        FNameEntry EntryToRead;
        EntryToRead.EntryOffset = UPKStream.tellg();
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameLength), 4);
        if (EntryToRead.NameLength > 0)
        {
            getline(UPKStream, EntryToRead.Name, '\0');
        }
        else
        {
            EntryToRead.Name = "";
        }
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameFlagsL), 4);
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameFlagsH), 4);
        EntryToRead.EntrySize = (unsigned)UPKStream.tellg() - EntryToRead.EntryOffset;
        NameTable.push_back(EntryToRead);
        if (EntryToRead.Name == "None")
            NoneIdx = i;
    }
    LogDebug("Reading ImportTable...");
    ImportTable.clear();
    UPKStream.seekg(Summary.ImportOffset);
    ImportTable.push_back(FObjectImport()); /// null object (default zero-initialization)
    for (unsigned i = 0; i < Summary.ImportCount; ++i)
    {
        FObjectImport EntryToRead;
        EntryToRead.EntryOffset = UPKStream.tellg();
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.PackageIdx), sizeof(EntryToRead.PackageIdx));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.TypeIdx), sizeof(EntryToRead.TypeIdx));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.OwnerRef), sizeof(EntryToRead.OwnerRef));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameIdx), sizeof(EntryToRead.NameIdx));
        EntryToRead.EntrySize = (unsigned)UPKStream.tellg() - EntryToRead.EntryOffset;
        ImportTable.push_back(EntryToRead);
    }
    LogDebug("Reading ExportTable...");
    ExportTable.clear();
    UPKStream.seekg(Summary.ExportOffset);
    ExportTable.push_back(FObjectExport()); /// null-object
    for (unsigned i = 0; i < Summary.ExportCount; ++i)
    {
        FObjectExport EntryToRead;
        EntryToRead.EntryOffset = UPKStream.tellg();
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.TypeRef), sizeof(EntryToRead.TypeRef));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ParentClassRef), sizeof(EntryToRead.ParentClassRef));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.OwnerRef), sizeof(EntryToRead.OwnerRef));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NameIdx), sizeof(EntryToRead.NameIdx));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ArchetypeRef), sizeof(EntryToRead.ArchetypeRef));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ObjectFlagsH), sizeof(EntryToRead.ObjectFlagsH));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ObjectFlagsL), sizeof(EntryToRead.ObjectFlagsL));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.SerialSize), sizeof(EntryToRead.SerialSize));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.SerialOffset), sizeof(EntryToRead.SerialOffset));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.ExportFlags), sizeof(EntryToRead.ExportFlags));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.NetObjectCount), sizeof(EntryToRead.NetObjectCount));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.GUID), sizeof(EntryToRead.GUID));
        UPKStream.read(reinterpret_cast<char*>(&EntryToRead.Unknown1), sizeof(EntryToRead.Unknown1));
        EntryToRead.NetObjects.resize(EntryToRead.NetObjectCount);
        if (EntryToRead.NetObjectCount > 0)
        {
            UPKStream.read(reinterpret_cast<char*>(EntryToRead.NetObjects.data()), EntryToRead.NetObjects.size()*4);
        }
        EntryToRead.EntrySize = (unsigned)UPKStream.tellg() - EntryToRead.EntryOffset;
        ExportTable.push_back(EntryToRead);
    }
    LogDebug("Reading DependsBuf...");
    DependsBuf.clear();
    DependsBuf.resize(Summary.SerialOffset - Summary.DependsOffset);
    if (DependsBuf.size() > 0)
    {
        UPKStream.read(DependsBuf.data(), DependsBuf.size());
    }
    /// resolve names
    LogDebug("Resolving ImportTable names...");
    for (unsigned i = 1; i < ImportTable.size(); ++i)
    {
        ImportTable[i].Name = IndexToName(ImportTable[i].NameIdx);
        ImportTable[i].FullName = ResolveFullName(-i);
        ImportTable[i].Type = IndexToName(ImportTable[i].TypeIdx);
        if (ImportTable[i].Type == "")
        {
            ImportTable[i].Type = "Class";
        }
    }
    LogDebug("Resolving ExportTable names...");
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        ExportTable[i].Name = IndexToName(ExportTable[i].NameIdx);
        ExportTable[i].FullName = ResolveFullName(i);
        ExportTable[i].Type = ObjRefToName(ExportTable[i].TypeRef);
        if (ExportTable[i].Type == "")
        {
            ExportTable[i].Type = "Class";
        }
    }
    LogDebug("Package header read successfully.");
    UPKFileSize = UPKStream.str().size();
    return true;
}

std::vector<char> UPKReader::SerializeSummary()
{
    std::stringstream ss;
    ss.write(reinterpret_cast<char*>(&Summary.Signature), 4);
    int32_t Ver = (Summary.LicenseeVersion << 16) + Summary.Version;
    ss.write(reinterpret_cast<char*>(&Ver), 4);
    ss.write(reinterpret_cast<char*>(&Summary.HeaderSize), 4);
    ss.write(reinterpret_cast<char*>(&Summary.FolderNameLength), 4);
    if (Summary.FolderNameLength > 0)
    {
        ss.write(Summary.FolderName.c_str(), Summary.FolderNameLength);
    }
    ss.write(reinterpret_cast<char*>(&Summary.PackageFlags), 4);
    ss.write(reinterpret_cast<char*>(&Summary.NameCount), 4);
    ss.write(reinterpret_cast<char*>(&Summary.NameOffset), 4);
    ss.write(reinterpret_cast<char*>(&Summary.ExportCount), 4);
    ss.write(reinterpret_cast<char*>(&Summary.ExportOffset), 4);
    ss.write(reinterpret_cast<char*>(&Summary.ImportCount), 4);
    ss.write(reinterpret_cast<char*>(&Summary.ImportOffset), 4);
    ss.write(reinterpret_cast<char*>(&Summary.DependsOffset), 4);
    ss.write(reinterpret_cast<char*>(&Summary.SerialOffset), 4);
    ss.write(reinterpret_cast<char*>(&Summary.Unknown2), 4);
    ss.write(reinterpret_cast<char*>(&Summary.Unknown3), 4);
    ss.write(reinterpret_cast<char*>(&Summary.Unknown4), 4);
    ss.write(reinterpret_cast<char*>(&Summary.GUID), sizeof(Summary.GUID));
    ss.write(reinterpret_cast<char*>(&Summary.GenerationsCount), 4);
    for (unsigned i = 0; i < Summary.GenerationsCount; ++i)
    {
        FGenerationInfo Entry = Summary.Generations[i];
        ss.write(reinterpret_cast<char*>(&Entry.ExportCount), 4);
        ss.write(reinterpret_cast<char*>(&Entry.NameCount), 4);
        ss.write(reinterpret_cast<char*>(&Entry.NetObjectCount), 4);
    }
    ss.write(reinterpret_cast<char*>(&Summary.EngineVersion), 4);
    ss.write(reinterpret_cast<char*>(&Summary.CookerVersion), 4);
    ss.write(reinterpret_cast<char*>(&Summary.CompressionFlags), 4);
    ss.write(reinterpret_cast<char*>(&Summary.NumCompressedChunks), 4);
    for (unsigned i = 0; i < Summary.NumCompressedChunks; ++i)
    {
        FCompressedChunk CompressedChunk = Summary.CompressedChunks[i];
        ss.write(reinterpret_cast<char*>(&CompressedChunk.UncompressedOffset), 4);
        ss.write(reinterpret_cast<char*>(&CompressedChunk.UncompressedSize), 4);
        ss.write(reinterpret_cast<char*>(&CompressedChunk.CompressedOffset), 4);
        ss.write(reinterpret_cast<char*>(&CompressedChunk.CompressedSize), 4);
    }
    if (Summary.UnknownDataChunk.size() > 0)
    {
        ss.write(Summary.UnknownDataChunk.data(), Summary.UnknownDataChunk.size());
    }
    std::vector<char> ret(ss.tellp());
    ss.read(ret.data(), ret.size());
    return ret;
}

std::vector<char> UPKReader::SerializeHeader()
{
    std::stringstream ss;
    std::vector<char> sVect = SerializeSummary();
    ss.write(sVect.data(), sVect.size());
    for (unsigned i = 0; i < Summary.NameCount; ++i)
    {
        FNameEntry Entry = NameTable[i];
        ss.write(reinterpret_cast<char*>(&Entry.NameLength), 4);
        if (Entry.NameLength > 0)
        {
            ss.write(Entry.Name.c_str(), Entry.NameLength);
        }
        ss.write(reinterpret_cast<char*>(&Entry.NameFlagsL), 4);
        ss.write(reinterpret_cast<char*>(&Entry.NameFlagsH), 4);
    }
    for (unsigned i = 1; i <= Summary.ImportCount; ++i)
    {
        FObjectImport Entry = ImportTable[i];
        ss.write(reinterpret_cast<char*>(&Entry.PackageIdx), sizeof(Entry.PackageIdx));
        ss.write(reinterpret_cast<char*>(&Entry.TypeIdx), sizeof(Entry.TypeIdx));
        ss.write(reinterpret_cast<char*>(&Entry.OwnerRef), sizeof(Entry.OwnerRef));
        ss.write(reinterpret_cast<char*>(&Entry.NameIdx), sizeof(Entry.NameIdx));
    }
    for (unsigned i = 1; i <= Summary.ExportCount; ++i)
    {
        FObjectExport Entry = ExportTable[i];
        ss.write(reinterpret_cast<char*>(&Entry.TypeRef), sizeof(Entry.TypeRef));
        ss.write(reinterpret_cast<char*>(&Entry.ParentClassRef), sizeof(Entry.ParentClassRef));
        ss.write(reinterpret_cast<char*>(&Entry.OwnerRef), sizeof(Entry.OwnerRef));
        ss.write(reinterpret_cast<char*>(&Entry.NameIdx), sizeof(Entry.NameIdx));
        ss.write(reinterpret_cast<char*>(&Entry.ArchetypeRef), sizeof(Entry.ArchetypeRef));
        ss.write(reinterpret_cast<char*>(&Entry.ObjectFlagsH), sizeof(Entry.ObjectFlagsH));
        ss.write(reinterpret_cast<char*>(&Entry.ObjectFlagsL), sizeof(Entry.ObjectFlagsL));
        ss.write(reinterpret_cast<char*>(&Entry.SerialSize), sizeof(Entry.SerialSize));
        ss.write(reinterpret_cast<char*>(&Entry.SerialOffset), sizeof(Entry.SerialOffset));
        ss.write(reinterpret_cast<char*>(&Entry.ExportFlags), sizeof(Entry.ExportFlags));
        ss.write(reinterpret_cast<char*>(&Entry.NetObjectCount), sizeof(Entry.NetObjectCount));
        ss.write(reinterpret_cast<char*>(&Entry.GUID), sizeof(Entry.GUID));
        ss.write(reinterpret_cast<char*>(&Entry.Unknown1), sizeof(Entry.Unknown1));
        if (Entry.NetObjectCount > 0)
        {
            ss.write(reinterpret_cast<char*>(Entry.NetObjects.data()), Entry.NetObjects.size()*4);
        }
    }
    if (DependsBuf.size() > 0)
    {
        ss.write(DependsBuf.data(), DependsBuf.size());
    }
    std::vector<char> ret(ss.tellp());
    ss.read(ret.data(), ret.size());
    return ret;
}

std::string UPKReader::IndexToName(UNameIndex idx)
{
    std::ostringstream ss;
    if (idx.NameTableIdx >= NameTable.size())
    {
        LogWarn("Bad NameTableIdx in IndexToName!");
        return "Error";
    }
    ss << NameTable[idx.NameTableIdx].Name;
    if (idx.Numeric > 0 && ss.str() != "None")
        ss << "_" << int(idx.Numeric - 1);
    return ss.str();
}

std::string UPKReader::ObjRefToName(UObjectReference ObjRef)
{
    if (-ObjRef >= (int)ImportTable.size() || ObjRef >= (int)ExportTable.size())
    {
        LogWarn("Bad ObjRef in ObjRefToName!");
        return "Error";
    }
    if (ObjRef == 0)
    {
        return "";
    }
    else if (ObjRef > 0)
    {
        return IndexToName(ExportTable[ObjRef].NameIdx);
    }
    else if (ObjRef < 0)
    {
        return IndexToName(ImportTable[-ObjRef].NameIdx);
    }
    return "";
}

UObjectReference UPKReader::GetOwnerRef(UObjectReference ObjRef)
{
    if (-ObjRef >= (int)ImportTable.size() || ObjRef >= (int)ExportTable.size())
    {
        LogWarn("Bad ObjRef in GetOwnerRef!");
        return 0;
    }
    if (ObjRef == 0)
    {
        return 0;
    }
    else if (ObjRef > 0)
    {
        return ExportTable[ObjRef].OwnerRef;
    }
    else if (ObjRef < 0)
    {
        return ImportTable[-ObjRef].OwnerRef;
    }
    return 0;
}

std::string UPKReader::ResolveFullName(UObjectReference ObjRef)
{
    std::string name;
    name = ObjRefToName(ObjRef);
    UObjectReference next = GetOwnerRef(ObjRef);
    while (next != 0)
    {
        name = ObjRefToName(next) + "." + name;
        next = GetOwnerRef(next);
    }
    return name;
}

int UPKReader::FindName(std::string name)
{
    for (unsigned i = 0; i < NameTable.size(); ++i)
    {
        if (NameTable[i].Name == name)
            return i;
    }
    return -1;
}

UObjectReference UPKReader::FindObjectMatchType(std::string FullName, std::string Type, bool isExport)
{
    /// Import object
    if (isExport == false)
    {
        for (unsigned i = 1; i < ImportTable.size(); ++i)
        {
            if (ImportTable[i].Type == Type && ImportTable[i].FullName == FullName)
                return -i;
        }
    }
    /// Export object
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        if (ExportTable[i].Type == Type && ExportTable[i].FullName == FullName)
            return i;
    }
    /// Object not found
    return 0;
}

UObjectReference UPKReader::FindObject(std::string FullName, bool isExport)
{
    /// Import object
    if (isExport == false)
    {
        for (unsigned i = 1; i < ImportTable.size(); ++i)
        {
            if (ImportTable[i].FullName == FullName)
                return -i;
        }
    }
    /// Export object
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        if (ExportTable[i].FullName == FullName)
            return i;
    }
    /// Object not found
    return 0;
}

UObjectReference UPKReader::FindObjectByName(std::string Name, bool isExport)
{
    /// Import object
    if (isExport == false)
    {
        for (unsigned i = 1; i < ImportTable.size(); ++i)
        {
            if (ImportTable[i].Name == Name)
                return -i;
        }
    }
    /// Export object
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        if (ExportTable[i].Name == Name)
            return i;
    }
    /// Object not found
    return 0;
}

UObjectReference UPKReader::FindObjectByOffset(size_t offset)
{
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        if (offset >= ExportTable[i].SerialOffset && offset < ExportTable[i].SerialOffset + ExportTable[i].SerialSize)
            return i;
    }
    return 0;
}

const FObjectExport& UPKReader::GetExportEntry(uint32_t idx)
{
    if (idx < ExportTable.size())
        return ExportTable[idx];
    else
    {
        LogWarn("Bad idx in GetExportEntry!");
        return ExportTable[0];
    }
}

const FObjectImport& UPKReader::GetImportEntry(uint32_t idx)
{
    if (idx < ImportTable.size())
        return ImportTable[idx];
    else
    {
        LogWarn("Bad idx in GetImportEntry!");
        return ImportTable[0];
    }
}

const FNameEntry& UPKReader::GetNameEntry(uint32_t idx)
{
    if (idx < NameTable.size())
        return NameTable[idx];
    else
    {
        LogWarn("Bad idx in GetNameEntry!");
        return NameTable[NoneIdx];
    }
}

std::vector<char> UPKReader::GetExportData(uint32_t idx)
{
    std::vector<char> data;
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in GetExportData!");
        return data;
    }
    data.resize(ExportTable[idx].SerialSize);
    UPKStream.seekg(ExportTable[idx].SerialOffset);
    UPKStream.read(data.data(), data.size());
    LastAccessedExportObjIdx = idx;
    return data;
}

std::vector<char> UPKReader::GetObjectTableData(UObjectReference ObjRef)
{
    std::vector<char> data;
    if (ObjRef == 0 || ObjRef >= (int)ExportTable.size() || -ObjRef >= (int)ImportTable.size())
    {
        LogWarn("Bad ObjRef in GetObjectTableData!");
        return data;
    }
    if (ObjRef > 0)
    {
        data.resize(ExportTable[ObjRef].EntrySize);
        UPKStream.seekg(ExportTable[ObjRef].EntryOffset);
    }
    else
    {
        data.resize(ImportTable[-ObjRef].EntrySize);
        UPKStream.seekg(ImportTable[-ObjRef].EntryOffset);
    }
    UPKStream.read(data.data(), data.size());
    return data;
}

UObject* UPKReader::GetExportObject(uint32_t idx, bool TryUnsafe, bool QuickMode)
{
    if (idx > 0 && idx < ExportTable.size())
    {
        if (ObjectsMap.count(idx) > 0 || Deserialize(idx, TryUnsafe, QuickMode))
        {
            return ObjectsMap[idx];
        }
    }
    LogWarn("Index is out of bounds in GetExportObject!");
    if (ObjectsMap.count(0) == 0)
    {
        ObjectsMap[0] = new UObjectNone; /// null object
    }
    return ObjectsMap[0];
}

bool UPKReader::Deserialize(uint32_t idx, bool TryUnsafe, bool QuickMode)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in Deserialize!");
        return false;
    }
    if (ObjectsMap.count(idx) > 0)
    {
        return true;
    }
    UObject* Obj;
    if (ExportTable[idx].ObjectFlagsH & (uint32_t)UObjectFlagsH::PropertiesObject)
    {
        Obj = UObjectFactory::Create(GlobalType::UObject);
    }
    else
    {
        Obj = UObjectFactory::Create(ExportTable[idx].Type);
    }
    if (Obj == nullptr)
    {
        LogWarn("Error creating an Object in Deserialize!");
        return false;
    }
    Obj->LinkPackage(PackageName, idx);
    Obj->SetParams(TryUnsafe, QuickMode);
    if (!Obj->Deserialize())
    {
        LogWarn("Error deserializing an Object in Deserialize!");
        delete Obj;
        return false;
    }
    ObjectsMap[idx] = Obj;
    return true;
}

size_t UPKReader::GetScriptSize(uint32_t idx)
{
    UObject* Obj = GetExportObject(idx, false, true);
    if (Obj->IsStructure() == false)
    {
        LogWarn("Object has no script in GetScriptSize!");
        return 0;
    }
    return Obj->GetScriptSerialSize();
}

size_t UPKReader::GetScriptMemSize(uint32_t idx)
{
    UObject* Obj = GetExportObject(idx, false, true);
    if (Obj->IsStructure() == false)
    {
        LogWarn("Object has no script in GetScriptSize!");
        return 0;
    }
    return Obj->GetScriptMemorySize();
}

size_t UPKReader::GetScriptRelOffset(uint32_t idx)
{
    UObject* Obj = GetExportObject(idx, false, true);
    if (Obj->IsStructure() == false)
    {
        LogWarn("Object has no script in GetScriptSize!");
        return 0;
    }
    return Obj->GetScriptOffset();
}

void UPKReader::SaveExportData(uint32_t idx, std::string outDir)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in SaveExportData!");
        return;
    }
    std::string filename = outDir + "/" + ExportTable[idx].FullName + "." + ExportTable[idx].Type;
    std::vector<char> dataChunk = GetExportData(idx);
    std::ofstream out(filename.c_str(), std::ios::binary);
    out.write(dataChunk.data(), dataChunk.size());
}

std::string UPKReader::FormatCompressedHeader()
{
    std::ostringstream ss;
    ss << "Signature: " << FormatHEX(CompressedHeader.Signature) << std::endl
       << "BlockSize: " << CompressedHeader.BlockSize << std::endl
       << "CompressedSize: " << CompressedHeader.CompressedSize << std::endl
       << "UncompressedSize: " << CompressedHeader.UncompressedSize << std::endl
       << "NumBlocks: " << CompressedHeader.NumBlocks << std::endl;
    for (unsigned i = 0; i < CompressedHeader.Blocks.size(); ++i)
    {
        ss << "Blocks[" << i << "]:" << std::endl
           << "\tCompressedSize: " << CompressedHeader.Blocks[i].CompressedSize << std::endl
           << "\tUncompressedSize: " << CompressedHeader.Blocks[i].UncompressedSize << std::endl;
    }
    return ss.str();
}

std::string UPKReader::FormatSummary()
{
    if (CompressedChunk == true)
        return FormatCompressedHeader();
    std::ostringstream ss;
    ss << "Signature: " << FormatHEX(Summary.Signature) << std::endl
       << "Version: " << Summary.Version << std::endl
       << "LicenseeVersion: " << Summary.LicenseeVersion << std::endl
       << "HeaderSize: " << Summary.HeaderSize << " (" << FormatHEX(Summary.HeaderSize) << ")" << std::endl
       << "Folder: " << Summary.FolderName << std::endl
       << "PackageFlags: " << FormatHEX(Summary.PackageFlags) << std::endl
       << FormatPackageFlags(Summary.PackageFlags)
       << "NameCount: " << Summary.NameCount << std::endl
       << "NameOffset: " << FormatHEX(Summary.NameOffset) << std::endl
       << "ExportCount: " << Summary.ExportCount << std::endl
       << "ExportOffset: " << FormatHEX(Summary.ExportOffset) << std::endl
       << "ImportCount: " << Summary.ImportCount << std::endl
       << "ImportOffset: " << FormatHEX(Summary.ImportOffset) << std::endl
       << "DependsOffset: " << FormatHEX(Summary.DependsOffset) << std::endl
       << "SerialOffset: " << FormatHEX(Summary.SerialOffset) << std::endl
       << "Unknown2: " << FormatHEX(Summary.Unknown2) << std::endl
       << "Unknown3: " << FormatHEX(Summary.Unknown3) << std::endl
       << "Unknown4: " << FormatHEX(Summary.Unknown4) << std::endl
       << "GUID: " << FormatHEX(Summary.GUID) << std::endl
       << "GenerationsCount: " << Summary.GenerationsCount << std::endl;
    for (unsigned i = 0; i < Summary.Generations.size(); ++i)
    {
        ss << "Generations[" << i << "]:" << std::endl
           << "\tExportCount: " << Summary.Generations[i].ExportCount << std::endl
           << "\tNameCount: " << Summary.Generations[i].NameCount << std::endl
           << "\tNetObjectCount: " << Summary.Generations[i].NetObjectCount << std::endl;
    }
    ss << "EngineVersion: " << Summary.EngineVersion << std::endl
       << "CookerVersion: " << Summary.CookerVersion << std::endl
       << "CompressionFlags: " << FormatHEX(Summary.CompressionFlags) << std::endl
       << FormatCompressionFlags(Summary.CompressionFlags)
       << "NumCompressedChunks: " << Summary.NumCompressedChunks << std::endl;
    for (unsigned i = 0; i < Summary.CompressedChunks.size(); ++i)
    {
        ss << "CompressedChunks[" << i << "]:" << std::endl
           << "\tUncompressedOffset: " << FormatHEX(Summary.CompressedChunks[i].UncompressedOffset) << " (" << Summary.CompressedChunks[i].UncompressedOffset << ")" << std::endl
           << "\tUncompressedSize: " << Summary.CompressedChunks[i].UncompressedSize << std::endl
           << "\tCompressedOffset: " << FormatHEX(Summary.CompressedChunks[i].CompressedOffset) << "(" << Summary.CompressedChunks[i].CompressedOffset << ")" << std::endl
           << "\tCompressedSize: " << Summary.CompressedChunks[i].CompressedSize << std::endl;
    }
    if (Summary.UnknownDataChunk.size() > 0)
    {
        ss << "Unknown data size: " << Summary.UnknownDataChunk.size() << std::endl;
        ss << "Unknown data: " << FormatHEX(Summary.UnknownDataChunk) << std::endl;
    }
    return ss.str();
}

std::string UPKReader::FormatNames(bool verbose)
{
    std::ostringstream ss;
    ss << "NameTable:" << std::endl;
    for (unsigned i = 0; i < NameTable.size(); ++i)
    {
        ss << FormatName(i, verbose);
    }
    return ss.str();
}

std::string UPKReader::FormatImports(bool verbose)
{
    std::ostringstream ss;
    ss << "ImportTable:" << std::endl;
    for (unsigned i = 1; i < ImportTable.size(); ++i)
    {
        ss << FormatImport(i, verbose);
    }
    return ss.str();
}

std::string UPKReader::FormatExports(bool verbose)
{
    std::ostringstream ss;
    ss << "ExportTable:" << std::endl;
    for (unsigned i = 1; i < ExportTable.size(); ++i)
    {
        ss << FormatExport(i, verbose);
    }
    return ss.str();
}

std::string UPKReader::FormatName(uint32_t idx, bool verbose)
{
    std::ostringstream ss;
    FNameEntry Entry = GetNameEntry(idx);
    ss << FormatHEX((uint32_t)idx) << " (" << idx << ") ( "
       << FormatHEX((char*)&idx, sizeof(idx)) << "): "
       << Entry.Name << std::endl;
    if (verbose == true)
    {
        ss << "\tNameFlagsL: " << FormatHEX(Entry.NameFlagsL) << std::endl
           << "\tNameFlagsH: " << FormatHEX(Entry.NameFlagsH) << std::endl;
    }
    return ss.str();
}

std::string UPKReader::FormatImport(uint32_t idx, bool verbose)
{
    std::ostringstream ss;
    int32_t invIdx = -idx;
    FObjectImport Entry = GetImportEntry(idx);
    ss << FormatHEX((uint32_t)(-idx)) << " (" << (-(int)idx) << ") ( "
       << FormatHEX((char*)&invIdx, sizeof(invIdx)) << "): "
       << Entry.Type << "\'"
       << Entry.FullName << "\'" << std::endl;
    if (verbose == true)
    {
        ss << "\tPackageIdx: " << FormatHEX(Entry.PackageIdx) << " -> " << IndexToName(Entry.PackageIdx) << std::endl
           << "\tTypeIdx: " << FormatHEX(Entry.TypeIdx) << " -> " << IndexToName(Entry.TypeIdx) << std::endl
           << "\tOwnerRef: " << FormatHEX((uint32_t)Entry.OwnerRef) << " -> " << ObjRefToName(Entry.OwnerRef) << std::endl
           << "\tNameIdx: " << FormatHEX(Entry.NameIdx) << " -> " << IndexToName(Entry.NameIdx) << std::endl;
    }
    return ss.str();
}

std::string UPKReader::FormatExport(uint32_t idx, bool verbose)
{
    std::ostringstream ss;
    FObjectExport Entry = GetExportEntry(idx);
    ss << FormatHEX((uint32_t)idx) << " (" << idx << ") ( "
       << FormatHEX((char*)&idx, sizeof(idx)) << "): "
       << Entry.Type << "\'"
       << Entry.FullName << "\'" << std::endl;
    if (verbose == true)
    {
        ss << "\tTypeRef: " << FormatHEX((uint32_t)Entry.TypeRef) << " -> " << ObjRefToName(Entry.TypeRef) << std::endl
           << "\tParentClassRef: " << FormatHEX((uint32_t)Entry.ParentClassRef) << " -> " << ObjRefToName(Entry.ParentClassRef) << std::endl
           << "\tOwnerRef: " << FormatHEX((uint32_t)Entry.OwnerRef) << " -> " << ObjRefToName(Entry.OwnerRef) << std::endl
           << "\tNameIdx: " << FormatHEX(Entry.NameIdx) << " -> " << IndexToName(Entry.NameIdx) << std::endl
           << "\tArchetypeRef: " << FormatHEX((uint32_t)Entry.ArchetypeRef) << " -> " << ObjRefToName(Entry.ArchetypeRef) << std::endl
           << "\tObjectFlagsH: " << FormatHEX(Entry.ObjectFlagsH) << std::endl
           << FormatObjectFlagsH(Entry.ObjectFlagsH)
           << "\tObjectFlagsL: " << FormatHEX(Entry.ObjectFlagsL) << std::endl
           << FormatObjectFlagsL(Entry.ObjectFlagsL)
           << "\tSerialSize: " << FormatHEX(Entry.SerialSize) << " (" << Entry.SerialSize << ")" << std::endl
           << "\tSerialOffset: " << FormatHEX(Entry.SerialOffset) << std::endl
           << "\tExportFlags: " << FormatHEX(Entry.ExportFlags) << std::endl
           << FormatExportFlags(Entry.ExportFlags)
           << "\tNetObjectCount: " << Entry.NetObjectCount << std::endl
           << "\tGUID: " << FormatHEX(Entry.GUID) << std::endl
           << "\tUnknown1: " << FormatHEX(Entry.Unknown1) << std::endl;
        for (unsigned i = 0; i < Entry.NetObjects.size(); ++i)
        {
            ss << "\tNetObjects[" << i << "]: " << FormatHEX(Entry.NetObjects[i]) << std::endl;
        }
    }
    return ss.str();
}

std::string FormatReadErrors(UPKReadErrors ReadError)
{
    std::ostringstream ss;
    switch(ReadError)
    {
    case UPKReadErrors::FileError:
        ss << "Bad package file!";
        break;
    case UPKReadErrors::BadSignature:
        ss << "Bad package signature!";
        break;
    case UPKReadErrors::BadVersion:
        ss << "Bad package version!";
        break;
    case UPKReadErrors::IsCompressed:
        ss << "Package is compressed, need to decompress first!";
        break;
    case UPKReadErrors::DecompressionError:
        ss << "Error decompressing the package!";
        break;
    case UPKReadErrors::Uninitialized:
        ss << "Package is not initialized, need to read package file first!";
        break;
    default:
        break;
    }
    return ss.str();
}
