#ifndef UDEFAULTPROPERTY_H
#define UDEFAULTPROPERTY_H

#include "UPKDeclarations.h"

class UDefaultProperty
{
public:
    UDefaultProperty() {}
    ~UDefaultProperty() {}
    void Init(UObject* owner, bool unsafe = false, bool quick = false) { Owner = owner; TryUnsafe = unsafe; QuickMode = quick; }
    bool Deserialize();
    bool DeserializeValue();
    std::string GetName() { return Name; }
    std::string FindArrayType();
    std::string GuessArrayType();
protected:
    friend class UDefaultPropertiesList;

    /// persistent
    UNameIndex NameIdx;
    UNameIndex TypeIdx;
    uint32_t PropertySize;
    uint32_t ArrayIdx;
    uint8_t  BoolValue;       /// for BoolProperty only
    UNameIndex InnerNameIdx;  /// for StructProperty and ByteProperty only
    /// memory
    std::string Name = "None";
    std::string Type = "None";
    UObject* Owner;
    bool TryUnsafe = false;
    bool QuickMode = false;
};

class UDefaultPropertiesList
{
public:
    UDefaultPropertiesList() {}
    ~UDefaultPropertiesList() {}
    bool Deserialize();
    void Init(UObject* owner) { Owner = owner; }
protected:
    std::vector<UDefaultProperty> DefaultProperties;
    size_t PropertyOffset = 0;
    size_t PropertySize = 0;
    UObject* Owner;
};

#endif // UDEFAULTPROPERTY_H
