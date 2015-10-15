#ifndef UPKREADER_H
#define UPKREADER_H

#include <iostream>
#include <sstream>
#include <map>

#include "UPKDeclarations.h"
#include "UFlags.h"
#include "LogService.h"

enum class UPKReadErrors
{
    NoErrors = 0,
    FileError,
    BadSignature,
    BadVersion,
    IsCompressed,
    DecompressionError,
    Uninitialized
};

class UPKReader
{
public:
    /// constructors
    UPKReader() {}
    explicit UPKReader(const char* filename);
    /// destructor
    ~UPKReader();
    /// Load package into memory, decompress if needed
    bool LoadPackage(const char* filename = nullptr);
    bool IsLoaded() { return (ReadError == UPKReadErrors::NoErrors); }
    /// read package header
    bool ReadPackageHeader();
    bool ReadCompressedHeader();
    bool ReinitializeHeader();
    /// Save uncompressed package to file
    bool SavePackage(const char* filename = nullptr);
    /// Extract serialized data
    void SaveExportData(uint32_t idx, std::string outDir = ".");
    /// Serialize package summary only (no tables)
    std::vector<char> SerializeSummary();
    /// Serialize the whole header (summary + tables)
    std::vector<char> SerializeHeader();
    /// Finders
    std::string IndexToName(UNameIndex idx);
    std::string ObjRefToName(UObjectReference ObjRef);
    std::string ResolveFullName(UObjectReference ObjRef);
    UObjectReference GetOwnerRef(UObjectReference ObjRef);
    int FindName(std::string name);
    UObjectReference FindObject(std::string FullName, bool isExport = true);
    UObjectReference FindObjectMatchType(std::string FullName, std::string Type, bool isExport = true);
    UObjectReference FindObjectByName(std::string Name, bool isExport = true);
    UObjectReference FindObjectByOffset(size_t offset);
    bool IsNoneIdx(UNameIndex idx) { return (idx.NameTableIdx == NoneIdx); }
    /// Entries
    std::string GetEntryName(UObjectReference ObjRef) { return (ObjRef < 0 ? GetImportEntry(-ObjRef).Name : GetExportEntry(ObjRef).Name); }
    std::string GetEntryFullName(UObjectReference ObjRef) { return (ObjRef < 0 ? GetImportEntry(-ObjRef).FullName : GetExportEntry(ObjRef).FullName); }
    std::string GetEntryType(UObjectReference ObjRef) { return (ObjRef < 0 ? GetImportEntry(-ObjRef).Type : GetExportEntry(ObjRef).Type); }
    UObjectReference GetEntryOwnerRef(UObjectReference ObjRef) { return (ObjRef < 0 ? GetImportEntry(-ObjRef).OwnerRef : GetExportEntry(ObjRef).OwnerRef); }
    std::string GetEntryOwnerName(UObjectReference ObjRef) { return GetEntryName(GetEntryOwnerRef(ObjRef)); }
    std::string GetEntryOwnerFullName(UObjectReference ObjRef) { return GetEntryFullName(GetEntryOwnerRef(ObjRef)); }
    std::string GetEntryPackage(UObjectReference ObjRef) { return (ObjRef < 0 ? IndexToName(GetImportEntry(-ObjRef).PackageIdx) : PackageName); }
    bool IsImportEntry(UObjectReference ObjRef) { return ObjRef < 0; }
    bool IsExportEntry(UObjectReference ObjRef) { return ObjRef > 0; }
    bool IsNullEntry(UObjectReference ObjRef) { return ObjRef == 0; }
    uint32_t GetEntrySerialSize(UObjectReference ObjRef) { return (ObjRef <= 0 ? 0 : GetExportEntry(ObjRef).SerialSize); }
    /// Getters
    const FObjectExport& GetExportEntry(uint32_t idx);
    const FObjectImport& GetImportEntry(uint32_t idx);
    const FNameEntry& GetNameEntry(uint32_t idx);
    const std::string& GetUPKFileName() { return UPKFileName; }
    const std::string& GetPackageName() { return PackageName; }
    const FPackageFileSummary& GetSummary() { return Summary; }
    const std::vector<FObjectExport>& GetExportTable() { return ExportTable; }
    const FGuid& GetGUID() { return Summary.GUID; }
    const UPKReadErrors& GetError() { return ReadError; }
    bool IsCompressed() { return Compressed; }
    bool IsLZOCompressed() { return Summary.CompressionFlags & (uint32_t)UCompressionFlags::LZO; }
    bool IsFullyCompressed() { return (Compressed && CompressedChunk); }
    const uint32_t& GetCompressionFlags() { return Summary.CompressionFlags; }
    const UObjectReference& GetLastAccessedExportObjIdx() { return LastAccessedExportObjIdx; }
    std::vector<char> GetExportData(uint32_t idx);
    std::vector<char> GetObjectTableData(UObjectReference ObjRef);
    UObject* GetExportObject(uint32_t idx, bool TryUnsafe = false, bool QuickMode = false);
    /// Deserialization
    bool Deserialize(uint32_t idx, bool TryUnsafe = false, bool QuickMode = false);
    size_t GetScriptSize(uint32_t idx);
    size_t GetScriptMemSize(uint32_t idx);
    size_t GetScriptRelOffset(uint32_t idx);
    /// Formatting
    std::string FormatCompressedHeader();
    std::string FormatSummary();
    std::string FormatNames(bool verbose = false);
    std::string FormatImports(bool verbose = false);
    std::string FormatExports(bool verbose = false);
    std::string FormatName(uint32_t idx, bool verbose = false);
    std::string FormatImport(uint32_t idx, bool verbose = false);
    std::string FormatExport(uint32_t idx, bool verbose = false);
    /// logging
    std::string MySenderName() { return mySenderName; }
protected:
    /// protected functions
    void LogErrorState(UPKReadErrors err);
    bool Decompress();
    friend bool DecompressLZOCompressedPackage(UPKReader *Package);
    void ClearObjects();
    /// protected member variables
    std::string UPKFileName = "";
    std::string PackageName = "";
    std::stringstream UPKStream;
    size_t UPKFileSize = 0;
    FPackageFileSummary Summary;
    std::vector<FNameEntry> NameTable;
    std::vector<FObjectImport> ImportTable;
    std::vector<FObjectExport> ExportTable;
    std::vector<char> DependsBuf;
    uint32_t NoneIdx = 0;
    UPKReadErrors ReadError = UPKReadErrors::Uninitialized;
    bool Compressed = false;
    bool CompressedChunk = false;
    FCompressedChunkHeader CompressedHeader;
    UObjectReference LastAccessedExportObjIdx = 0;
    std::map<uint32_t, UObject*> ObjectsMap;
    /// ProgLog sender name
    std::string mySenderName = "UPKReader";
};

std::string FormatReadErrors(UPKReadErrors ReadError);

#endif // UPKREADER_H
