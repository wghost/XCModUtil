#ifndef UPKUTILS_H
#define UPKUTILS_H

#include "UPKReader.h"
#include "UObjectFactory.h"

class UPKUtils: public UPKReader
{
public:
    /// constructors & destructor
    UPKUtils() { mySenderName = "UPKUtils"; }
    explicit UPKUtils(const char* filename): UPKReader(filename) { mySenderName = "UPKUtils"; }
    ~UPKUtils() {}
    /// Move/expand export object data (legacy functions for backward compatibility)
    bool MoveExportData(uint32_t idx, uint32_t newObjectSize = 0);
    bool UndoMoveExportData(uint32_t idx);
    /// Move/resize export object data (new functions)
    /// You cannot resize object without moving it first
    /// You can move object without resizing it
    std::vector<char> GetResizedDataChunk(uint32_t idx, int newObjectSize = -1, int resizeAt = -1);
    bool ResizeInPlace(uint32_t idx, int newObjectSize = -1, int resizeAt = -1);
    bool MoveResizeObject(uint32_t idx, int newObjectSize = -1, int resizeAt = -1);
    bool UndoMoveResizeObject(uint32_t idx);
    /// Deserialize
    bool Deserialize(FNameEntry& entry, std::vector<char>& data);
    bool Deserialize(FObjectImport& entry, std::vector<char>& data);
    bool Deserialize(FObjectExport& entry, std::vector<char>& data);
    /// Write data
    bool CheckValidFileOffset(size_t offset);
    bool WriteExportData(uint32_t idx, std::vector<char> data, std::vector<char> *backupData = nullptr);
    bool WriteNameTableName(uint32_t idx, std::string name);
    bool WriteData(size_t offset, std::vector<char> data, std::vector<char> *backupData = nullptr);
    size_t FindDataChunk(std::vector<char> data, size_t beg = 0, size_t limit = 0);
    std::vector<char> GetBulkData(size_t offset, std::vector<char> data);
    /// Aggressive patching functions
    bool AddNameEntry(FNameEntry Entry);
    bool AddImportEntry(FObjectImport Entry);
    bool AddExportEntry(FObjectExport Entry);
    bool LinkChild(UObjectReference OwnerRef, UObjectReference ChildRef);
};

#endif // UPKUTILS_H
