#ifndef UOBJECT_H
#define UOBJECT_H

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <utility>

#include "UPKDeclarations.h"
#include "UDefaultProperty.h"
#include "TextUtils.h"

/// global type enumeration
enum class GlobalType
{
	None            =  0,
	UObject         =  1,
	UField          =  2,
	UConst          =  3,
	UEnum           =  4,
	UProperty       =  5,
	UByteProperty   =  6,
	UIntProperty    =  7,
	UBoolProperty   =  8,
	UFloatProperty  =  9,
	UObjectProperty = 10,
	UClassProperty  = 11,
	UNameProperty   = 12,
	UStructProperty = 13,
	UStrProperty    = 14,
	UArrayProperty  = 15,
	UStruct         = 16,
	UScriptStruct   = 17,
	UFunction       = 18,
	UState          = 19,
	UClass          = 20,
	UTextBuffer     = 21,
	UObjectUnknown  = 22,
	UFixedArrayProperty  = 23,
	UComponentProperty = 24,
	UDelegateProperty = 25,
	UInterfaceProperty = 26,
	UMapProperty = 27,
	ULevel = 28
};

class UBulkDataMirror
{
public:
    UBulkDataMirror() {}
    void SetBulkData(std::vector<char> Data);
    void SetFileOffset(size_t offset) { SavedBulkDataOffsetInFile = offset; }
    std::string Serialize();
    size_t GetBulkDataRelOffset() { return 16; }
protected:
    /// persistent
    uint32_t SavedBulkDataFlags = 0;
    uint32_t SavedElementCount = 0;
    uint32_t SavedBulkDataSizeOnDisk = 0;
    uint32_t SavedBulkDataOffsetInFile = 0;
    std::vector<char> BulkData;
};

/// parent class of all Unreal objects
class UObject
{
public:
    UObject() {}
    virtual ~UObject() {}
    bool LinkPackage(std::string name, uint32_t idx);
    void SetParams(bool unsafe = false, bool quick = false) { TryUnsafe = unsafe; QuickMode = quick; }
    /// deserialization interface
    virtual bool Deserialize();
    /// object type
    virtual bool IsStructure() { return false; }
    virtual bool IsProperty() { return false; }
    virtual bool IsState() { return false; }
    /// getters
    std::string GetText() { return Text.str(); }
    virtual UObjectReference GetNextRef() { return 0; }
    virtual size_t GetNextRefOffset() { return 0; }
    virtual UObjectReference GetFirstChildRef() { return 0; }
    virtual uint32_t GetScriptSerialSize() { return 0; }
    virtual uint32_t GetScriptMemorySize() { return 0; }
    virtual size_t GetScriptOffset() { return 0; }
    virtual size_t GetFirstChildRefOffset() { return 0; }
    virtual UObjectReference GetInner() { return 0; }
    virtual UObjectReference GetStructObjRef() { return 0; }
protected:
    friend class UDefaultPropertiesList;
    friend class UDefaultProperty;
    /// persistent
    int32_t NetIndex = 0;
    UDefaultPropertiesList DefaultProperties; /// for non-Class objects only
    /// memory
    GlobalType Type = GlobalType::UObject;
    std::string Package = "";
    uint32_t Index = 0;
    UPKReader* Reader = nullptr;
    std::istringstream stream;
    bool TryUnsafe = false;
    bool QuickMode = false;
    bool Initialized = false;
    /// deserialized text
    std::ostringstream Text;
    /// relative binary data offsets
    size_t FlagsOffset = 0;
    /// helpers
    bool IsComponent();
    bool IsDominantDirectionalLightComponent();
    bool IsDefaultPropertiesObject();
    bool IsSubobject();
};

class UObjectNone: public UObject
{
public:
    UObjectNone() { Type = GlobalType::None; }
    ~UObjectNone() {}
protected:
};

class UField: public UObject
{
public:
    UField() { Type = GlobalType::UField; }
    ~UField() {}
    virtual bool Deserialize();
    virtual UObjectReference GetNextRef() { return NextRef; }
    virtual size_t GetNextRefOffset() { return NextRefOffset; }
protected:
    /// persistent
    UObjectReference NextRef;
    UObjectReference ParentRef; /// for Struct objects only
    /// memory
    size_t FieldOffset;
    size_t FieldSize;
    size_t NextRefOffset;
};

class UStruct: public UField
{
public:
    UStruct() { Type = GlobalType::UStruct; }
    ~UStruct() {}
    virtual bool Deserialize();
    virtual bool IsStructure() { return true; }
    virtual UObjectReference GetFirstChildRef() { return FirstChildRef; }
    virtual uint32_t GetScriptSerialSize() { return ScriptSerialSize; }
    virtual uint32_t GetScriptMemorySize() { return ScriptMemorySize; }
    virtual size_t GetScriptOffset() { return ScriptOffset; }
    virtual size_t GetFirstChildRefOffset() { return FirstChildRefOffset; }
protected:
    /// persistent
    UObjectReference ScriptTextRef;
    UObjectReference FirstChildRef;
    UObjectReference CppTextRef;
    uint32_t Line;
    uint32_t TextPos;
    uint32_t ScriptMemorySize;
    uint32_t ScriptSerialSize;
    std::vector<char> DataScript;
    /// memory
    size_t StructOffset;
    size_t StructSize;
    size_t ScriptOffset;
    size_t FirstChildRefOffset;
};

class UFunction: public UStruct
{
public:
    UFunction() { Type = GlobalType::UFunction; }
    ~UFunction() {}
    virtual bool Deserialize();
protected:
    /// persistent
    uint16_t NativeToken;
    uint8_t OperPrecedence;
    uint32_t FunctionFlags;
    uint16_t RepOffset;
    UNameIndex NameIdx;
    /// memory
    size_t FunctionOffset;
    size_t FunctionSize;
};

class UScriptStruct: public UStruct
{
public:
    UScriptStruct() { Type = GlobalType::UScriptStruct; }
    ~UScriptStruct() {}
    virtual bool Deserialize();
protected:
    /// persistent
    uint32_t StructFlags;
    UDefaultPropertiesList StructDefaultProperties;
    /// memory
    size_t ScriptStructOffset;
    size_t ScriptStructSize;
};

class UState: public UStruct
{
public:
    UState() { Type = GlobalType::UState; }
    ~UState() {}
    virtual bool Deserialize();
    virtual bool IsState() { return true; }
protected:
    /// persistent
    uint32_t ProbeMask;
    uint16_t LabelTableOffset;
    uint32_t StateFlags;
    uint32_t StateMapSize;
    std::vector<std::pair<UNameIndex, UObjectReference> > StateMap;
    /// memory
    size_t StateOffset;
    size_t StateSize;
};

class UClass: public UState
{
public:
    UClass() { Type = GlobalType::UClass; }
    ~UClass() {}
    virtual bool Deserialize();
protected:
    /// persistent
    uint32_t ClassFlags;
    UObjectReference WithinRef;
    UNameIndex ConfigNameIdx;
    uint32_t NumComponents;
    std::vector<std::pair<UNameIndex, UObjectReference> > Components;
    uint32_t NumInterfaces;
    std::vector<std::pair<UObjectReference, uint32_t> > Interfaces;
    uint32_t NumDontSortCategories;
    std::vector<UNameIndex> DontSortCategories;
    uint32_t NumHideCategories;
    std::vector<UNameIndex> HideCategories;
    uint32_t NumAutoExpandCategories;
    std::vector<UNameIndex> AutoExpandCategories;
    uint32_t NumAutoCollapseCategories;
    std::vector<UNameIndex> AutoCollapseCategories;
    uint32_t ForceScriptOrder;
    uint32_t NumClassGroups;
    std::vector<UNameIndex> ClassGroups;
    uint32_t NativeClassNameLength;
    std::string NativeClassName;
    UNameIndex DLLBindName;
    UObjectReference DefaultRef;
};

class UConst: public UField
{
public:
    UConst() { Type = GlobalType::UConst; }
    ~UConst() {}
    virtual bool Deserialize();
protected:
    /// persistent
    uint32_t ValueLength;
    std::string Value;
};

class UEnum: public UField
{
public:
    UEnum() { Type = GlobalType::UEnum; }
    ~UEnum() {}
    virtual bool Deserialize();
protected:
    /// persistent
    uint32_t NumNames;
    std::vector<UNameIndex> Names;
};

class UProperty: public UField
{
public:
    UProperty() { Type = GlobalType::UProperty; }
    ~UProperty() {}
    virtual bool Deserialize();
    virtual bool IsProperty() { return true; }
protected:
    /// persistent
    uint16_t ArrayDim;
    uint16_t ElementSize;
    uint32_t PropertyFlagsL;
    uint32_t PropertyFlagsH;
    UNameIndex CategoryIndex;
    UObjectReference ArrayEnumRef;
    uint16_t RepOffset;
};

class UByteProperty: public UProperty
{
public:
    UByteProperty() { Type = GlobalType::UByteProperty; }
    ~UByteProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference EnumObjRef;
};

class UIntProperty: public UProperty
{
public:
    UIntProperty() { Type = GlobalType::UIntProperty; }
    ~UIntProperty() {}
protected:
};

class UBoolProperty: public UProperty
{
public:
    UBoolProperty() { Type = GlobalType::UBoolProperty; }
    ~UBoolProperty() {}
protected:
};

class UFloatProperty: public UProperty
{
public:
    UFloatProperty() { Type = GlobalType::UFloatProperty; }
    ~UFloatProperty() {}
protected:
};

class UObjectProperty: public UProperty
{
public:
    UObjectProperty() { Type = GlobalType::UObjectProperty; }
    ~UObjectProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference OtherObjRef;
};

class UClassProperty: public UObjectProperty
{
public:
    UClassProperty() { Type = GlobalType::UClassProperty; }
    ~UClassProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference ClassObjRef;
};

class UComponentProperty: public UObjectProperty
{
public:
    UComponentProperty() { Type = GlobalType::UComponentProperty; }
    ~UComponentProperty() {}
protected:
};

class UNameProperty: public UProperty
{
public:
    UNameProperty() { Type = GlobalType::UNameProperty; }
    ~UNameProperty() {}
protected:
};

class UStructProperty: public UProperty
{
public:
    UStructProperty() { Type = GlobalType::UStructProperty; }
    ~UStructProperty() {}
    virtual bool Deserialize();
    virtual UObjectReference GetStructObjRef() { return StructObjRef; }
protected:
    /// persistent
    UObjectReference StructObjRef;
};

class UStrProperty: public UProperty
{
public:
    UStrProperty() { Type = GlobalType::UStrProperty; }
    ~UStrProperty() {}
protected:
};

class UFixedArrayProperty: public UProperty
{
public:
    UFixedArrayProperty() { Type = GlobalType::UFixedArrayProperty; }
    ~UFixedArrayProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference InnerObjRef;
    uint32_t Count;
};

class UArrayProperty: public UProperty
{
public:
    UArrayProperty() { Type = GlobalType::UArrayProperty; }
    ~UArrayProperty() {}
    virtual bool Deserialize();
    virtual UObjectReference GetInner() { return InnerObjRef; }
protected:
    /// persistent
    UObjectReference InnerObjRef;
};

class UDelegateProperty: public UProperty
{
public:
    UDelegateProperty() { Type = GlobalType::UDelegateProperty; }
    ~UDelegateProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference FunctionObjRef;
    UObjectReference DelegateObjRef;
};

class UInterfaceProperty: public UProperty
{
public:
    UInterfaceProperty() { Type = GlobalType::UInterfaceProperty; }
    ~UInterfaceProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference InterfaceObjRef;
};

class UMapProperty: public UProperty
{
public:
    UMapProperty() { Type = GlobalType::UMapProperty; }
    ~UMapProperty() {}
    virtual bool Deserialize();
protected:
    /// persistent
    UObjectReference KeyObjRef;
    UObjectReference ValueObjRef;
};

class ULevel: public UObject
{
public:
    ULevel() { Type = GlobalType::ULevel; }
    ~ULevel() {}
    virtual bool Deserialize();
protected:
    /// database
    std::vector<UObjectReference> Actors;
};

class UObjectUnknown: public UObject
{
public:
    UObjectUnknown() { Type = GlobalType::UObjectUnknown; }
    ~UObjectUnknown() {}
    virtual bool Deserialize();
protected:
};

#endif // UOBJECT_H
