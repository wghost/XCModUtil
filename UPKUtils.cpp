#include "UPKUtils.h"

#include <cstring>
#include <sstream>

uint8_t PatchUPKhash [] = {0x7A, 0xA0, 0x56, 0xC9,
                           0x60, 0x5F, 0x7B, 0x31,
                           0x72, 0x5D, 0x4B, 0xC4,
                           0x7C, 0xD2, 0x4D, 0xD9 };


/// relatively safe behavior (old realization)
bool UPKUtils::MoveExportData(uint32_t idx, uint32_t newObjectSize)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in MoveExportData!");
        return false;
    }
    std::vector<char> data = GetExportData(idx);
    UPKStream.seekg(0, std::ios::end);
    uint32_t newObjectOffset = UPKStream.tellg();
    bool isFunction = (ExportTable[idx].Type == "Function");
    if (newObjectSize > ExportTable[idx].SerialSize)
    {
        UPKStream.seekp(ExportTable[idx].EntryOffset + sizeof(uint32_t)*8);
        UPKStream.write(reinterpret_cast<char*>(&newObjectSize), sizeof(newObjectSize));
        unsigned int diffSize = newObjectSize - data.size();
        if (isFunction == false)
        {
            for (unsigned int i = 0; i < diffSize; ++i)
            {
                LogWarn("Object is not a function in MoveExportData!");
                data.push_back(0x00);
            }
        }
        else
        {
            uint32_t oldMemSize = 0;
            uint32_t oldFileSize = 0;
            memcpy(&oldMemSize, data.data() + 0x28, 0x4);  /// copy function memory size
            memcpy(&oldFileSize, data.data() + 0x2C, 0x4); /// and file size
            uint32_t newMemSize = oldMemSize + diffSize;   /// find new sizes
            uint32_t newFileSize = oldFileSize + diffSize;
            uint32_t headSize = 0x30 + oldFileSize - 1;    /// head size (all data before 0x53)
            uint32_t tailSize = ExportTable[idx].SerialSize - headSize; /// tail size (0x53 and all data after)
            std::vector<char> newData(newObjectSize);
            memset(newData.data(), 0x0B, newObjectSize);     /// fill new data with 0x0B
            memcpy(newData.data(), data.data(), headSize);   /// copy all data before 0x53
            memcpy(newData.data() + 0x28, &newMemSize, 0x4); /// set new memory size
            memcpy(newData.data() + 0x2C, &newFileSize, 0x4);/// and file size
            memcpy(newData.data() + headSize + diffSize, data.data() + headSize, tailSize); /// copy 0x53 and all data after
            data = newData;
        }
    }
    UPKStream.seekp(ExportTable[idx].EntryOffset + sizeof(uint32_t)*9);
    UPKStream.write(reinterpret_cast<char*>(&newObjectOffset), sizeof(newObjectOffset));
    UPKStream.seekp(newObjectOffset);
    UPKStream.write(data.data(), data.size());
    /// write backup info
    UPKStream.write(reinterpret_cast<char*>(&PatchUPKhash[0]), 16);
    UPKStream.write(reinterpret_cast<char*>(&ExportTable[idx].SerialSize), sizeof(ExportTable[idx].SerialSize));
    UPKStream.write(reinterpret_cast<char*>(&ExportTable[idx].SerialOffset), sizeof(ExportTable[idx].SerialOffset));
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::UndoMoveExportData(uint32_t idx)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in UndoMoveExportData!");
        return false;
    }
    UPKStream.seekg(ExportTable[idx].SerialOffset + ExportTable[idx].SerialSize);
    uint8_t readHash [16];
    UPKStream.read(reinterpret_cast<char*>(&readHash[0]), 16);
    if (memcmp(readHash, PatchUPKhash, 16) != 0)
    {
        LogWarn("PatchUPKhash is missing in UndoMoveExportData!");
        return false;
    }
    uint32_t oldObjectFileSize, oldObjectOffset;
    UPKStream.read(reinterpret_cast<char*>(&oldObjectFileSize), sizeof(oldObjectFileSize));
    UPKStream.read(reinterpret_cast<char*>(&oldObjectOffset), sizeof(oldObjectOffset));
    UPKStream.seekp(ExportTable[idx].EntryOffset + sizeof(uint32_t)*8);
    UPKStream.write(reinterpret_cast<char*>(&oldObjectFileSize), sizeof(oldObjectFileSize));
    UPKStream.write(reinterpret_cast<char*>(&oldObjectOffset), sizeof(oldObjectOffset));
    /// reinitialize
    ReinitializeHeader();
    return true;
}

std::vector<char> UPKUtils::GetResizedDataChunk(uint32_t idx, int newObjectSize, int resizeAt)
{
    std::vector<char> data;
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in GetResizedDataChunk!");
        return data;
    }
    /// get export object serial data
    data = GetExportData(idx);
    /// if object needs resizing
    if (newObjectSize > 0 && (unsigned)newObjectSize != data.size())
    {
        /// if resizing occurs in the middle of an object
        if (resizeAt > 0 && resizeAt < newObjectSize)
        {
            int diff = newObjectSize - data.size();
            std::vector<char> newData(newObjectSize);
            memset(newData.data(), 0, newObjectSize); /// fill with zeros
            memcpy(newData.data(), data.data(), resizeAt); /// copy head
            if (diff > 0) /// if expanding
                memcpy(newData.data() + resizeAt + diff, data.data() + resizeAt, data.size() - resizeAt);
            else /// if shrinking
                memcpy(newData.data() + resizeAt, data.data() + resizeAt - diff, data.size() - (resizeAt - diff));
            data = newData;
        }
        else
        {
            data.resize(newObjectSize, 0);
        }
    }
    return data;
}

bool UPKUtils::MoveResizeObject(uint32_t idx, int newObjectSize, int resizeAt)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in MoveResizeObject!");
        return false;
    }
    std::vector<char> data = GetResizedDataChunk(idx, newObjectSize, resizeAt);
    /// move write pointer to the end of file
    UPKStream.seekg(0, std::ios::end);
    uint32_t newObjectOffset = UPKStream.tellg();
    /// if object needs resizing
    if (ExportTable[idx].SerialSize != data.size())
    {
        /// write new SerialSize to ExportTable entry
        UPKStream.seekp(ExportTable[idx].EntryOffset + sizeof(uint32_t)*8);
        UPKStream.write(reinterpret_cast<char*>(&newObjectSize), sizeof(newObjectSize));
    }
    /// write new SerialOffset to ExportTable entry
    UPKStream.seekp(ExportTable[idx].EntryOffset + sizeof(uint32_t)*9);
    UPKStream.write(reinterpret_cast<char*>(&newObjectOffset), sizeof(newObjectOffset));
    /// write new SerialData
    if (data.size() > 0)
    {
        UPKStream.seekp(newObjectOffset);
        UPKStream.write(data.data(), data.size());
    }
    /// write backup info
    UPKStream.write(reinterpret_cast<char*>(&PatchUPKhash[0]), 16);
    UPKStream.write(reinterpret_cast<char*>(&ExportTable[idx].SerialSize), sizeof(ExportTable[idx].SerialSize));
    UPKStream.write(reinterpret_cast<char*>(&ExportTable[idx].SerialOffset), sizeof(ExportTable[idx].SerialOffset));
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::UndoMoveResizeObject(uint32_t idx)
{
    return UndoMoveExportData(idx);
}

bool UPKUtils::CheckValidFileOffset(size_t offset)
{
    if (IsLoaded() == false)
    {
        return false;
    }
    /// guard package signature and version
    return (offset >= 8 && offset < UPKFileSize);
}

bool UPKUtils::WriteExportData(uint32_t idx, std::vector<char> data, std::vector<char> *backupData)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in WriteExportData!");
        return false;
    }
    if (!IsLoaded())
    {
        LogWarn("Package is not loaded in WriteExportData!");
        return false;
    }
    if (ExportTable[idx].SerialSize != data.size())
    {
        LogWarn("Export object size != data chunk size in WriteExportData!");
        return false;
    }
    if (backupData != nullptr)
    {
        backupData->clear();
        backupData->resize(data.size());
        UPKStream.seekg(ExportTable[idx].SerialOffset);
        UPKStream.read(backupData->data(), backupData->size());
    }
    UPKStream.seekp(ExportTable[idx].SerialOffset);
    UPKStream.write(data.data(), data.size());
    return true;
}

bool UPKUtils::WriteNameTableName(uint32_t idx, std::string name)
{
    if (idx < 1 || idx >= NameTable.size())
    {
        LogWarn("Index is out of bounds in WriteNameTableName!");
        return false;
    }
    if (!IsLoaded())
    {
        LogWarn("Package is not loaded in WriteNameTableName!");
        return false;
    }
    if ((unsigned)(NameTable[idx].NameLength - 1) != name.length())
    {
        LogWarn("Name length != new name length in WriteNameTableName!");
        return false;
    }
    UPKStream.seekp(NameTable[idx].EntryOffset + sizeof(NameTable[idx].NameLength));
    UPKStream.write(name.c_str(), name.length());
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::WriteData(size_t offset, std::vector<char> data, std::vector<char> *backupData)
{
    if (!CheckValidFileOffset(offset))
    {
        LogWarn("File offset is out of bounds in WriteData!");
        return false;
    }
    if (backupData != nullptr)
    {
        backupData->clear();
        backupData->resize(data.size());
        UPKStream.seekg(offset);
        UPKStream.read(backupData->data(), backupData->size());
    }
    UPKStream.seekp(offset);
    UPKStream.write(data.data(), data.size());
    /// reinitialize
    ReinitializeHeader();
    return true;
}

std::vector<char> UPKUtils::GetBulkData(size_t offset, std::vector<char> data)
{
    UBulkDataMirror DataMirror;
    DataMirror.SetBulkData(data);
    DataMirror.SetFileOffset(offset + DataMirror.GetBulkDataRelOffset());
    std::string mirrorStr = DataMirror.Serialize();
    std::vector<char> mirrorVec(mirrorStr.size());
    memcpy(mirrorVec.data(), mirrorStr.data(), mirrorStr.size());
    return mirrorVec;
}

size_t UPKUtils::FindDataChunk(std::vector<char> data, size_t beg, size_t limit)
{
    if (limit != 0 && (limit - beg + 1 < data.size() || limit < beg))
    {
        LogWarn("Invalid input params in FindDataChunk!");
        return 0;
    }

    std::string bufStr = UPKStream.str().substr(beg, (limit == 0 ? UPKFileSize : limit) - beg + 1);
    std::string dataStr(data.data(), data.size());

    size_t pos = bufStr.find(dataStr);
    if (pos != std::string::npos)
    {
        return pos + beg;
    }
    return 0;
}

bool UPKUtils::ResizeInPlace(uint32_t idx, int newObjectSize, int resizeAt)
{
    if (idx < 1 || idx >= ExportTable.size())
    {
        LogWarn("Index is out of bounds in ResizeInPlace!");
        return false;
    }
    std::vector<char> data = GetResizedDataChunk(idx, newObjectSize, resizeAt);
    int diffSize = data.size() - ExportTable[idx].SerialSize;
    /// recalculate offsets
    for (unsigned i = 1; i <= Summary.ExportCount; ++i)
    {
        if (i != idx && ExportTable[i].SerialOffset > ExportTable[idx].SerialOffset)
        {
            ExportTable[i].SerialOffset += diffSize;
        }
    }
    /// backup serialized export data
    UPKStream.clear();
    UPKStream.seekg(Summary.SerialOffset);
    std::vector<char> serializedDataBeforeIdx(ExportTable[idx].SerialOffset - UPKStream.tellg());
    if (serializedDataBeforeIdx.size() > 0)
    {
        UPKStream.read(serializedDataBeforeIdx.data(), serializedDataBeforeIdx.size());
    }
    UPKStream.seekg(ExportTable[idx].SerialOffset + ExportTable[idx].SerialSize);
    std::vector<char> serializedDataAfterIdx(UPKFileSize - UPKStream.tellg());
    if (serializedDataAfterIdx.size() > 0)
    {
        UPKStream.read(serializedDataAfterIdx.data(), serializedDataAfterIdx.size());
    }
    /// save new serial size
    ExportTable[idx].SerialSize = newObjectSize;
    /// serialize header
    std::vector<char> serializedHeader = SerializeHeader();
    /// rewrite package
    UPKStream.str("");
    /// write serialized header
    UPKStream.write(serializedHeader.data(), serializedHeader.size());
    /// write serialized export data before resized object
    if (serializedDataBeforeIdx.size() > 0)
    {
        UPKStream.write(serializedDataBeforeIdx.data(), serializedDataBeforeIdx.size());
    }
    /// write resized export object data
    UPKStream.write(data.data(), data.size());
    /// write serialized export data after resized object
    if (serializedDataAfterIdx.size() > 0)
    {
        UPKStream.write(serializedDataAfterIdx.data(), serializedDataAfterIdx.size());
    }
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::AddNameEntry(FNameEntry Entry)
{
    size_t oldSerialOffset = Summary.SerialOffset;
    /// increase header size
    Summary.HeaderSize += Entry.EntrySize;
    /// add entry
    ++Summary.NameCount;
    NameTable.push_back(Entry);
    /// increase offsets
    Summary.ImportOffset += Entry.EntrySize;
    Summary.ExportOffset += Entry.EntrySize;
    Summary.DependsOffset += Entry.EntrySize;
    Summary.SerialOffset += Entry.EntrySize;
    for (unsigned i = 1; i <= Summary.ExportCount; ++i)
    {
        ExportTable[i].SerialOffset += Entry.EntrySize;
    }
    /// backup serialized export data into memory
    UPKStream.clear();
    UPKStream.seekg(oldSerialOffset);
    std::vector<char> serializedData(UPKFileSize - oldSerialOffset);
    UPKStream.read(serializedData.data(), serializedData.size());
    /// rewrite package
    UPKStream.seekp(0);
    /// serialize header
    std::vector<char> serializedHeader = SerializeHeader();
    /// write serialized header
    UPKStream.write(serializedHeader.data(), serializedHeader.size());
    /// write serialized export data
    UPKStream.write(serializedData.data(), serializedData.size());
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::AddImportEntry(FObjectImport Entry)
{
    size_t oldSerialOffset = Summary.SerialOffset;
    /// increase header size
    Summary.HeaderSize += Entry.EntrySize;
    /// add entry
    ++Summary.ImportCount;
    ImportTable.push_back(Entry);
    /// increase offsets
    Summary.ExportOffset += Entry.EntrySize;
    Summary.DependsOffset += Entry.EntrySize;
    Summary.SerialOffset += Entry.EntrySize;
    for (unsigned i = 1; i <= Summary.ExportCount; ++i)
    {
        ExportTable[i].SerialOffset += Entry.EntrySize;
    }
    /// backup serialized export data into memory
    UPKStream.clear();
    UPKStream.seekg(oldSerialOffset);
    std::vector<char> serializedData(UPKFileSize - oldSerialOffset);
    UPKStream.read(serializedData.data(), serializedData.size());
    /// rewrite package
    UPKStream.seekp(0);
    /// serialize header
    std::vector<char> serializedHeader = SerializeHeader();
    /// write serialized header
    UPKStream.write(serializedHeader.data(), serializedHeader.size());
    /// write serialized export data
    UPKStream.write(serializedData.data(), serializedData.size());
    /// reinitialize
    ReinitializeHeader();
    return true;
}

bool UPKUtils::AddExportEntry(FObjectExport Entry)
{
    unsigned oldExportCount = Summary.ExportCount;
    size_t oldSerialOffset = Summary.SerialOffset;
    /// increase header size
    Summary.HeaderSize += Entry.EntrySize;
    /// add entry
    ++Summary.ExportCount;
    if (Entry.SerialSize < 16) /// PrevObject + NoneIdx + NextRef
    {
        Entry.SerialSize = 16;
    }
    Entry.SerialOffset = UPKFileSize + Entry.EntrySize;
    ExportTable.push_back(Entry);
    /// increase offsets
    Summary.DependsOffset += Entry.EntrySize;
    Summary.SerialOffset += Entry.EntrySize;
    for (unsigned i = 1; i <= oldExportCount; ++i)
    {
        ExportTable[i].SerialOffset += Entry.EntrySize;
    }
    /// backup serialized export data into memory
    UPKStream.clear();
    UPKStream.seekg(oldSerialOffset);
    std::vector<char> serializedData(UPKFileSize - oldSerialOffset);
    UPKStream.read(serializedData.data(), serializedData.size());
    /// rewrite package
    UPKStream.seekp(0);
    /// serialize header
    std::vector<char> serializedHeader = SerializeHeader();
    /// write serialized header
    UPKStream.write(serializedHeader.data(), serializedHeader.size());
    /// write serialized export data
    UPKStream.write(serializedData.data(), serializedData.size());
    /// write new export serialized data
    std::vector<char> serializedEntry(Entry.SerialSize);
    UObjectReference PrevObjRef = oldExportCount;
    memcpy(serializedEntry.data(), reinterpret_cast<char*>(&PrevObjRef), sizeof(PrevObjRef));
    memcpy(serializedEntry.data() + sizeof(PrevObjRef), reinterpret_cast<char*>(&NoneIdx), sizeof(NoneIdx));
    UPKStream.write(serializedEntry.data(), serializedEntry.size());
    /// reinitialize
    ReinitializeHeader();
    /// link export object to owner
    LinkChild(Entry.OwnerRef, Summary.ExportCount);
    return true;
}

bool UPKUtils::LinkChild(UObjectReference OwnerRef, UObjectReference ChildRef)
{
    if (OwnerRef < 1 || OwnerRef >= (int)ExportTable.size())
    {
        LogWarn("Index is out of bounds in LinkChild!");
        return false;
    }
    UObject* Obj = GetExportObject(OwnerRef, false, true);
    if (!Obj->IsStructure())
    {
        LogWarn("Object is not a structure in LinkChild!");
        return false;
    }
    UObjectReference FirstChildRef = Obj->GetFirstChildRef();
    /// owner has no children
    if (FirstChildRef == 0)
    {
        /// link child to owner
        UPKStream.seekg(Obj->GetFirstChildRefOffset() + ExportTable[OwnerRef].SerialOffset);
        UPKStream.write(reinterpret_cast<char*>(&ChildRef), sizeof(ChildRef));
        return true;
    }
    /// find last child
    UObjectReference NextRef = FirstChildRef;
    size_t LastRefOffset = 0;
    while (NextRef != 0)
    {
        Obj = GetExportObject(NextRef, false, true);
        NextRef = Obj->GetNextRef();
        LastRefOffset = Obj->GetNextRefOffset() + ExportTable[NextRef].SerialOffset;
    }
    /// link new child to last child
    if (LastRefOffset != 0)
    {
        UPKStream.seekg(LastRefOffset);
        UPKStream.write(reinterpret_cast<char*>(&ChildRef), sizeof(ChildRef));
    }
    return true;
}

bool UPKUtils::Deserialize(FNameEntry& entry, std::vector<char>& data)
{
    if (data.size() < 12)
    {
        LogWarn("Bad data in Deserialize FNameEntry!");
        return false;
    }
    std::stringstream ss;
    ss.write(data.data(), data.size());
    ss.read(reinterpret_cast<char*>(&entry.NameLength), 4);
    if (12U + entry.NameLength != data.size())
    {
        LogWarn("Bad data in Deserialize FNameEntry!");
        return false;
    }
    if (entry.NameLength > 0)
    {
        getline(ss, entry.Name, '\0');
    }
    else
    {
        entry.Name = "";
    }
    ss.read(reinterpret_cast<char*>(&entry.NameFlagsL), 4);
    ss.read(reinterpret_cast<char*>(&entry.NameFlagsH), 4);
    /// memory variables
    entry.EntrySize = data.size();
    return true;
}

bool UPKUtils::Deserialize(FObjectImport& entry, std::vector<char>& data)
{
    if (data.size() != 28)
    {
        LogWarn("Bad data in Deserialize FObjectImport!");
        return false;
    }
    std::stringstream ss;
    ss.write(data.data(), data.size());
    ss.read(reinterpret_cast<char*>(&entry.PackageIdx), sizeof(entry.PackageIdx));
    ss.read(reinterpret_cast<char*>(&entry.TypeIdx), sizeof(entry.TypeIdx));
    ss.read(reinterpret_cast<char*>(&entry.OwnerRef), sizeof(entry.OwnerRef));
    ss.read(reinterpret_cast<char*>(&entry.NameIdx), sizeof(entry.NameIdx));
    /// memory variables
    entry.EntrySize = data.size();
    entry.Name = IndexToName(entry.NameIdx);
    entry.FullName = entry.Name;
    if (entry.OwnerRef != 0)
    {
        entry.FullName = ResolveFullName(entry.OwnerRef) + "." + entry.Name;
    }
    entry.Type = IndexToName(entry.TypeIdx);
    if (entry.Type == "")
    {
        entry.Type = "Class";
    }
    return true;
}

bool UPKUtils::Deserialize(FObjectExport& entry, std::vector<char>& data)
{
    if (data.size() < 68)
    {
        LogWarn("Bad data in Deserialize FObjectExport!");
        return false;
    }
    std::stringstream ss;
    ss.write(data.data(), data.size());
    ss.read(reinterpret_cast<char*>(&entry.TypeRef), sizeof(entry.TypeRef));
    ss.read(reinterpret_cast<char*>(&entry.ParentClassRef), sizeof(entry.ParentClassRef));
    ss.read(reinterpret_cast<char*>(&entry.OwnerRef), sizeof(entry.OwnerRef));
    ss.read(reinterpret_cast<char*>(&entry.NameIdx), sizeof(entry.NameIdx));
    ss.read(reinterpret_cast<char*>(&entry.ArchetypeRef), sizeof(entry.ArchetypeRef));
    ss.read(reinterpret_cast<char*>(&entry.ObjectFlagsH), sizeof(entry.ObjectFlagsH));
    ss.read(reinterpret_cast<char*>(&entry.ObjectFlagsL), sizeof(entry.ObjectFlagsL));
    ss.read(reinterpret_cast<char*>(&entry.SerialSize), sizeof(entry.SerialSize));
    ss.read(reinterpret_cast<char*>(&entry.SerialOffset), sizeof(entry.SerialOffset));
    ss.read(reinterpret_cast<char*>(&entry.ExportFlags), sizeof(entry.ExportFlags));
    ss.read(reinterpret_cast<char*>(&entry.NetObjectCount), sizeof(entry.NetObjectCount));
    ss.read(reinterpret_cast<char*>(&entry.GUID), sizeof(entry.GUID));
    ss.read(reinterpret_cast<char*>(&entry.Unknown1), sizeof(entry.Unknown1));
    if (68U + entry.NetObjectCount != data.size())
    {
        LogWarn("Bad data in Deserialize FObjectExport!");
        return false;
    }
    entry.NetObjects.resize(entry.NetObjectCount);
    if (entry.NetObjectCount > 0)
    {
        ss.read(reinterpret_cast<char*>(entry.NetObjects.data()), entry.NetObjects.size()*4);
    }
    /// memory variables
    entry.EntrySize = data.size();
    entry.Name = IndexToName(entry.NameIdx);
    entry.FullName = entry.Name;
    if (entry.OwnerRef != 0)
    {
        entry.FullName = ResolveFullName(entry.OwnerRef) + "." + entry.Name;
    }
    entry.Type = ObjRefToName(entry.TypeRef);
    if (entry.Type == "")
    {
        entry.Type = "Class";
    }
    return true;
}
