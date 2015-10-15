#ifndef UPKDECLARATIONS_H
#define UPKDECLARATIONS_H

#include <vector>
#include <string>

/// forward declaration of all the base classes

class UPKReader;
class UPKUtils;
class UObject;
class UObjectNone;
class UPackageManager;
class UDefaultProperty;
class UDefaultPropertiesList;

/// declaration of all the base data types

typedef int32_t UObjectReference;

struct FGuid
{
    /// persistent
    uint32_t GUID_A = 0;
    uint32_t GUID_B = 0;
    uint32_t GUID_C = 0;
    uint32_t GUID_D = 0;
};

struct FGenerationInfo
{
    int32_t ExportCount = 0;
    int32_t NameCount = 0;
    int32_t NetObjectCount = 0;
};

struct FCompressedChunkBlock
{
    uint32_t CompressedSize = 0;
    uint32_t UncompressedSize = 0;
};

struct FCompressedChunkHeader
{
    uint32_t Signature = 0;          // equals to PACKAGE_FILE_TAG (0x9E2A83C1)
    uint32_t BlockSize = 0;          // max size of uncompressed block, always the same
    uint32_t CompressedSize = 0;
    uint32_t UncompressedSize = 0;
    uint32_t NumBlocks = 0;
    std::vector<FCompressedChunkBlock> Blocks;
};

struct FCompressedChunk
{
    uint32_t UncompressedOffset = 0;
    uint32_t UncompressedSize = 0;
    uint32_t CompressedOffset = 0;
    uint32_t CompressedSize = 0;
};

struct UNameIndex
{
    uint32_t NameTableIdx = 0;
    uint32_t Numeric = 0;
};

struct FPackageFileSummary
{
    /// persistent
    uint32_t Signature = 0;
    uint16_t Version = 0;
    uint16_t LicenseeVersion = 0;
    uint32_t HeaderSize = 0;
    int32_t FolderNameLength = 0;
    std::string FolderName = "";
    uint32_t PackageFlags = 0;
    uint32_t NameCount = 0;
    uint32_t NameOffset = 0;
    uint32_t ExportCount = 0;
    uint32_t ExportOffset = 0;
    uint32_t ImportCount = 0;
    uint32_t ImportOffset = 0;
    uint32_t DependsOffset = 0;
    uint32_t SerialOffset = 0;
    uint32_t Unknown2 = 0;
    uint32_t Unknown3 = 0;
    uint32_t Unknown4 = 0;
    FGuid   GUID;
    uint32_t GenerationsCount = 0;
    std::vector<FGenerationInfo> Generations;
    uint32_t EngineVersion = 0;
    uint32_t CookerVersion = 0;
    uint32_t CompressionFlags = 0;
    uint32_t NumCompressedChunks = 0;
    std::vector<FCompressedChunk> CompressedChunks;
    std::vector<char> UnknownDataChunk;
    /// memory
    size_t HeaderSizeOffset = 0;
    size_t NameCountOffset = 0;
    size_t UPKFileSize = 0;
};

struct FNameEntry
{
    /// persistent
    int32_t     NameLength = 5;
    std::string Name = "None";
    uint32_t    NameFlagsL = 0;
    uint32_t    NameFlagsH = 0;
    /// memory
    size_t      EntryOffset = 0;
    size_t      EntrySize = 0;
};

struct FObjectImport
{
    /// persistent
    UNameIndex       PackageIdx;
    UNameIndex       TypeIdx;
    UObjectReference OwnerRef = 0;
    UNameIndex       NameIdx;
    /// memory
    size_t           EntryOffset = 0;
    size_t           EntrySize = 0;
    std::string      Name = "None";
    std::string      FullName = "None";
    std::string      Type = "None";
};

struct FObjectExport
{
    /// persistent
    UObjectReference TypeRef = 0;
    UObjectReference ParentClassRef = 0;
    UObjectReference OwnerRef = 0;
    UNameIndex       NameIdx;
    UObjectReference ArchetypeRef = 0;
    uint32_t         ObjectFlagsH = 0;
    uint32_t         ObjectFlagsL = 0;
    uint32_t         SerialSize = 0;
    uint32_t         SerialOffset = 0;
    uint32_t         ExportFlags = 0;
    uint32_t         NetObjectCount = 0;
    FGuid            GUID;
    uint32_t         Unknown1 = 0;
    std::vector<uint32_t> NetObjects;     // 4 x NetObjectCount bytes of data
    /// memory
    size_t           EntryOffset = 0;
    size_t           EntrySize = 0;
    std::string      Name = "None";
    std::string      FullName = "None";
    std::string      Type = "None";
};

#endif //UPKDECLARATIONS_H
