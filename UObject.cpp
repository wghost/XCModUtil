#include "UObject.h"
#include "UPKReader.h"
#include "LogService.h"
#include "UPackageManager.h"

#include <sstream>
#include <vector>

void UBulkDataMirror::SetBulkData(std::vector<char> Data)
{
    BulkData = Data;
    SavedBulkDataFlags = 0;
    /// temporarily, until we know the exact meaning of this
    SavedElementCount = SavedBulkDataSizeOnDisk = BulkData.size();
    SavedBulkDataOffsetInFile = 0;
}

std::string UBulkDataMirror::Serialize()
{
    std::stringstream ss;
    ss.write(reinterpret_cast<char*>(&SavedBulkDataFlags), 4);
    ss.write(reinterpret_cast<char*>(&SavedElementCount), 4);
    ss.write(reinterpret_cast<char*>(&SavedBulkDataSizeOnDisk), 4);
    ss.write(reinterpret_cast<char*>(&SavedBulkDataOffsetInFile), 4);
    if (BulkData.size() > 0)
    {
        ss.write(BulkData.data(), BulkData.size());
    }
    return ss.str();
}

bool UObject::LinkPackage(std::string name, uint32_t idx)
{
    Package = name;
    Index = idx;
    Reader = _FindPackage(Package).ReaderPtr;
    if (Reader == nullptr)
    {
        _LogError("Cannot find a package " + Package, "UObject");
        return false;
    }
    std::vector<char> data = Reader->GetExportData(Index);
    if (data.size() == 0)
    {
        _LogError("Bad export data! Object index = " + Index, "UObject");
        return false;
    }
    stream.str(std::string(data.data(), data.size()));
    Initialized = true;
    return true;
}

bool UObject::IsComponent()
{
    /// some hacky heuristic here
    return ((Reader->GetEntryType(Index).find("Component") != std::string::npos ||
             Reader->GetEntryType(Index).find("Distribution") != std::string::npos) &&
            Reader->GetEntryType(Index).find("MaterialExpression") == std::string::npos &&
            Reader->GetEntryType(Index) != "ComponentProperty");
}

bool UObject::IsDominantDirectionalLightComponent()
{
    return (Reader->GetEntryType(Index).find("DominantDirectionalLightComponent") != std::string::npos);
}

bool UObject::IsDefaultPropertiesObject()
{
    return (Reader->GetEntryName(Index).find("Default__") != std::string::npos);
}

bool UObject::IsSubobject()
{
    return (Reader->GetEntryOwnerFullName(Index).find("Default__") != std::string::npos);
}

bool UObject::Deserialize()
{
    Text.str(""); /// clear deserialized text
    FObjectExport ThisTableEntry = Reader->GetExportEntry(Index);
    /// for non default properties objects
    if (!IsDefaultPropertiesObject())
    {
        if (IsDominantDirectionalLightComponent())
        {
            Text << "DominantDirectionalLightComponent:\n";
            uint32_t DominantLightShadowMapSize;
            stream.read(reinterpret_cast<char*>(&DominantLightShadowMapSize), sizeof(DominantLightShadowMapSize));
            Text << "\tDominantLightShadowMapSize = " << FormatHEX((uint32_t)DominantLightShadowMapSize) << " = " << DominantLightShadowMapSize << std::endl;
            stream.seekg(2*DominantLightShadowMapSize, std::ios::cur);
            _LogDebug("Skipping DominantLightShadowMap." , "UObject");
            Text << "Cannot deserialize DominantLightShadowMap: skipping!\n";
        }
        if (IsComponent())
        {
            Text << "UComponent:\n";
            uint32_t TemplateOwnerClass;
            stream.read(reinterpret_cast<char*>(&TemplateOwnerClass), sizeof(TemplateOwnerClass));
            Text << "\tTemplateOwnerClass = " << FormatHEX((uint32_t)TemplateOwnerClass) << " = " << TemplateOwnerClass << " = " << Reader->ObjRefToName(TemplateOwnerClass) << std::endl;
            if (IsSubobject())
            {
                UNameIndex TemplateName;
                stream.read(reinterpret_cast<char*>(&TemplateName), sizeof(TemplateName));
                Text << "\tTemplateName = " << FormatHEX(TemplateName) << " = " << Reader->IndexToName(TemplateName) << std::endl;
            }
        }
    }
    Text << "UObject:\n";
    stream.read(reinterpret_cast<char*>(&NetIndex), sizeof(NetIndex));
    Text << "\tNetIndex = " << FormatHEX((uint32_t)NetIndex) << " = " << NetIndex << std::endl;
    if (Type != GlobalType::UClass)
    {
        if (TryUnsafe == true)
        {
            if (ThisTableEntry.ObjectFlagsL & (uint32_t)UObjectFlagsL::HasStack)
            {
                stream.seekg(22, std::ios::cur);
                _LogDebug("Skipping stack." , "UObject");
                Text << "Cannot deserialize stack: skipping!\n";
            }
        }
        DefaultProperties.Init(this);
        _LogDebug("Deserializing default properties." , "UObject");
        if (!DefaultProperties.Deserialize())
            return false;
    }
    return true;
}

bool UField::Deserialize()
{
    if (!UObject::Deserialize())
        return false;
    Text << "UField:\n";
    FieldOffset = NextRefOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&NextRef), sizeof(NextRef));
    Text << "\tNextRef = " << FormatHEX((uint32_t)NextRef) << " -> " << Reader->ObjRefToName(NextRef) << std::endl;
    if (IsStructure())
    {
        stream.read(reinterpret_cast<char*>(&ParentRef), sizeof(ParentRef));
        Text << "\tParentRef = " << FormatHEX((uint32_t)ParentRef) << " -> " << Reader->ObjRefToName(ParentRef) << std::endl;
    }
    FieldSize = (unsigned)stream.tellg() - (unsigned)FieldOffset;
    return true;
}

bool UStruct::Deserialize()
{
    if (!UField::Deserialize())
        return false;
    Text << "UStruct:\n";
    StructOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&ScriptTextRef), sizeof(ScriptTextRef));
    Text << "\tScriptTextRef = " << FormatHEX((uint32_t)ScriptTextRef) << " -> " << Reader->ObjRefToName(ScriptTextRef) << std::endl;
    FirstChildRefOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&FirstChildRef), sizeof(FirstChildRef));
    Text << "\tFirstChildRef = " << FormatHEX((uint32_t)FirstChildRef) << " -> " << Reader->ObjRefToName(FirstChildRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&CppTextRef), sizeof(CppTextRef));
    Text << "\tCppTextRef = " << FormatHEX((uint32_t)CppTextRef) << " -> " << Reader->ObjRefToName(CppTextRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&Line), sizeof(Line));
    Text << "\tLine = " << FormatHEX(Line) << std::endl;
    stream.read(reinterpret_cast<char*>(&TextPos), sizeof(TextPos));
    Text << "\tTextPos = " << FormatHEX(TextPos) << std::endl;
    stream.read(reinterpret_cast<char*>(&ScriptMemorySize), sizeof(ScriptMemorySize));
    Text << "\tScriptMemorySize = " << FormatHEX(ScriptMemorySize) << std::endl;
    stream.read(reinterpret_cast<char*>(&ScriptSerialSize), sizeof(ScriptSerialSize));
    Text << "\tScriptSerialSize = " << FormatHEX(ScriptSerialSize) << std::endl;
    if (ScriptSerialSize > 0xFFFF)
        return false;
    DataScript.resize(ScriptSerialSize);
    ScriptOffset = stream.tellg();
    if (ScriptSerialSize > 0)
    {
        stream.read(DataScript.data(), DataScript.size());
        Text << "\tSkipping script bytecode.\n";
    }
    StructSize = (unsigned)stream.tellg() - (unsigned)StructOffset;
    return true;
}

bool UFunction::Deserialize()
{
    if (!UStruct::Deserialize())
        return false;
    Text << "UFunction:\n";
    FunctionOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&NativeToken), sizeof(NativeToken));
    Text << "\tNativeToken = " << FormatHEX(NativeToken) << std::endl;
    stream.read(reinterpret_cast<char*>(&OperPrecedence), sizeof(OperPrecedence));
    Text << "\tOperPrecedence = " << FormatHEX(OperPrecedence) << std::endl;
    FlagsOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&FunctionFlags), sizeof(FunctionFlags));
    Text << "\tFunctionFlags = " << FormatHEX(FunctionFlags) << std::endl;
    Text << FormatFunctionFlags(FunctionFlags);
    if (FunctionFlags & (uint32_t)UFunctionFlags::Net)
    {
        stream.read(reinterpret_cast<char*>(&RepOffset), sizeof(RepOffset));
        Text << "\tRepOffset = " << FormatHEX(RepOffset) << std::endl;
    }
    stream.read(reinterpret_cast<char*>(&NameIdx), sizeof(NameIdx));
    Text << "\tNameIdx = " << FormatHEX(NameIdx) << " -> " << Reader->IndexToName(NameIdx) << std::endl;
    FunctionSize = (unsigned)stream.tellg() - (unsigned)FunctionOffset;
    return true;
}

bool UScriptStruct::Deserialize()
{
    if (!UStruct::Deserialize())
        return false;
    Text << "UScriptStruct:\n";
    ScriptStructOffset = stream.tellg();
    FlagsOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&StructFlags), sizeof(StructFlags));
    Text << "\tStructFlags = " << FormatHEX(StructFlags) << std::endl;
    Text << FormatStructFlags(StructFlags);
    StructDefaultProperties.Init(this);
    if (!StructDefaultProperties.Deserialize())
        return false;
    ScriptStructSize = (unsigned)stream.tellg() - (unsigned)ScriptStructOffset;
    return true;
}

bool UState::Deserialize()
{
    if (!UStruct::Deserialize())
        return false;
    Text << "UState:\n";
    StateOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&ProbeMask), sizeof(ProbeMask));
    Text << "\tProbeMask = " << FormatHEX(ProbeMask) << std::endl;
    stream.read(reinterpret_cast<char*>(&LabelTableOffset), sizeof(LabelTableOffset));
    Text << "\tLabelTableOffset = " << FormatHEX(LabelTableOffset) << std::endl;
    FlagsOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&StateFlags), sizeof(StateFlags));
    Text << "\tStateFlags = " << FormatHEX(StateFlags) << std::endl;
    Text << FormatStateFlags(StateFlags);
    stream.read(reinterpret_cast<char*>(&StateMapSize), sizeof(StateMapSize));
    Text << "\tStateMapSize = " << FormatHEX(StateMapSize) << " (" << StateMapSize << ")" << std::endl;
    StateMap.clear();
    if (StateMapSize * 12 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        StateMapSize = 0;
    for (unsigned i = 0; i < StateMapSize; ++i)
    {
        std::pair<UNameIndex, UObjectReference> MapElement;
        stream.read(reinterpret_cast<char*>(&MapElement), sizeof(MapElement));
        Text << "\tStateMap[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(MapElement.first) << " -> " << Reader->IndexToName(MapElement.first) << std::endl;
        Text << "\t\t" << FormatHEX((uint32_t)MapElement.second) << " -> " << Reader->ObjRefToName(MapElement.second) << std::endl;
        StateMap.push_back(MapElement);
    }
    StateSize = (unsigned)stream.tellg() - (unsigned)StateOffset;
    return true;
}

bool UClass::Deserialize()
{
    if (!UState::Deserialize())
        return false;
    Text << "UClass:\n";
    FlagsOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&ClassFlags), sizeof(ClassFlags));
    Text << "\tClassFlags = " << FormatHEX(ClassFlags) << std::endl;
    Text << FormatClassFlags(ClassFlags);
    stream.read(reinterpret_cast<char*>(&WithinRef), sizeof(WithinRef));
    Text << "\tWithinRef = " << FormatHEX((uint32_t)WithinRef) << " -> " << Reader->ObjRefToName(WithinRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&ConfigNameIdx), sizeof(ConfigNameIdx));
    Text << "\tConfigNameIdx = " << FormatHEX(ConfigNameIdx) << " -> " << Reader->IndexToName(ConfigNameIdx) << std::endl;
    stream.read(reinterpret_cast<char*>(&NumComponents), sizeof(NumComponents));
    Text << "\tNumComponents = " << FormatHEX(NumComponents) << " (" << NumComponents << ")" << std::endl;
    Components.clear();
    if (NumComponents * 12 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumComponents = 0;
    for (unsigned i = 0; i < NumComponents; ++i)
    {
        std::pair<UNameIndex, UObjectReference> MapElement;
        stream.read(reinterpret_cast<char*>(&MapElement), sizeof(MapElement));
        Text << "\tComponents[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(MapElement.first) << " -> " << Reader->IndexToName(MapElement.first) << std::endl;
        Text << "\t\t" << FormatHEX((uint32_t)MapElement.second) << " -> " << Reader->ObjRefToName(MapElement.second) << std::endl;
        Components.push_back(MapElement);
    }
    stream.read(reinterpret_cast<char*>(&NumInterfaces), sizeof(NumInterfaces));
    Text << "\tNumInterfaces = " << FormatHEX(NumInterfaces) << " (" << NumInterfaces << ")" << std::endl;
    Interfaces.clear();
    if (NumInterfaces * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumInterfaces = 0;
    for (unsigned i = 0; i < NumInterfaces; ++i)
    {
        std::pair<UObjectReference, uint32_t> MapElement;
        stream.read(reinterpret_cast<char*>(&MapElement), sizeof(MapElement));
        Text << "\tInterfaces[" << i << "]:\n";
        Text << "\t\t" << FormatHEX((uint32_t)MapElement.first) << " -> " << Reader->ObjRefToName(MapElement.first) << std::endl;
        Text << "\t\t" << FormatHEX(MapElement.second) << std::endl;
        Interfaces.push_back(MapElement);
    }
    stream.read(reinterpret_cast<char*>(&NumDontSortCategories), sizeof(NumDontSortCategories));
    Text << "\tNumDontSortCategories = " << FormatHEX(NumDontSortCategories) << " (" << NumDontSortCategories << ")" << std::endl;
    DontSortCategories.clear();
    if (NumDontSortCategories * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumDontSortCategories = 0;
    for (unsigned i = 0; i < NumDontSortCategories; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tDontSortCategories[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        DontSortCategories.push_back(Element);
    }
    stream.read(reinterpret_cast<char*>(&NumHideCategories), sizeof(NumHideCategories));
    Text << "\tNumHideCategories = " << FormatHEX(NumHideCategories) << " (" << NumHideCategories << ")" << std::endl;
    HideCategories.clear();
    if (NumHideCategories * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumHideCategories = 0;
    for (unsigned i = 0; i < NumHideCategories; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tHideCategories[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        HideCategories.push_back(Element);
    }
    stream.read(reinterpret_cast<char*>(&NumAutoExpandCategories), sizeof(NumAutoExpandCategories));
    Text << "\tNumAutoExpandCategories = " << FormatHEX(NumAutoExpandCategories) << " (" << NumAutoExpandCategories << ")" << std::endl;
    AutoExpandCategories.clear();
    if (NumAutoExpandCategories * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumAutoExpandCategories = 0;
    for (unsigned i = 0; i < NumAutoExpandCategories; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tAutoExpandCategories[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        AutoExpandCategories.push_back(Element);
    }
    stream.read(reinterpret_cast<char*>(&NumAutoCollapseCategories), sizeof(NumAutoCollapseCategories));
    Text << "\tNumAutoCollapseCategories = " << FormatHEX(NumAutoCollapseCategories) << " (" << NumAutoCollapseCategories << ")" << std::endl;
    AutoCollapseCategories.clear();
    if (NumAutoCollapseCategories * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumAutoCollapseCategories = 0;
    for (unsigned i = 0; i < NumAutoCollapseCategories; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tAutoCollapseCategories[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        AutoCollapseCategories.push_back(Element);
    }
    stream.read(reinterpret_cast<char*>(&ForceScriptOrder), sizeof(ForceScriptOrder));
    Text << "\tForceScriptOrder = " << FormatHEX(ForceScriptOrder) << std::endl;
    stream.read(reinterpret_cast<char*>(&NumClassGroups), sizeof(NumClassGroups));
    Text << "\tNumClassGroups = " << FormatHEX(NumClassGroups) << " (" << NumClassGroups << ")" << std::endl;
    ClassGroups.clear();
    if (NumClassGroups * 8 > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NumClassGroups = 0;
    for (unsigned i = 0; i < NumClassGroups; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tClassGroups[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        ClassGroups.push_back(Element);
    }
    stream.read(reinterpret_cast<char*>(&NativeClassNameLength), sizeof(NativeClassNameLength));
    Text << "\tNativeClassNameLength = " << FormatHEX(NativeClassNameLength) << std::endl;
    if (NativeClassNameLength > Reader->GetExportEntry(Index).SerialSize) /// bad data malloc error prevention
        NativeClassNameLength = 0;
    if (NativeClassNameLength > 0)
    {
        getline(stream, NativeClassName, '\0');
        Text << "\tNativeClassName = " << NativeClassName << std::endl;
    }
    stream.read(reinterpret_cast<char*>(&DLLBindName), sizeof(DLLBindName));
    Text << "\tDLLBindName = " << FormatHEX(DLLBindName) << " -> " << Reader->IndexToName(DLLBindName) << std::endl;
    stream.read(reinterpret_cast<char*>(&DefaultRef), sizeof(DefaultRef));
    Text << "\tDefaultRef = " << FormatHEX((uint32_t)DefaultRef) << " -> " << Reader->ObjRefToName(DefaultRef) << std::endl;
    return true;
}

bool UConst::Deserialize()
{
    if (!UField::Deserialize())
        return false;
    Text << "UConst:\n";
    stream.read(reinterpret_cast<char*>(&ValueLength), sizeof(ValueLength));
    Text << "\tValueLength = " << FormatHEX(ValueLength) << std::endl;
    if (ValueLength > 0)
    {
        getline(stream, Value, '\0');
        Text << "\tValue = " << Value << std::endl;
    }
    return true;
}

bool UEnum::Deserialize()
{
    if (!UField::Deserialize())
        return false;
    Text << "UEnum:\n";
    stream.read(reinterpret_cast<char*>(&NumNames), sizeof(NumNames));
    Text << "\tNumNames = " << FormatHEX(NumNames) << " (" << NumNames << ")" << std::endl;
    Names.clear();
    for (unsigned i = 0; i < NumNames; ++i)
    {
        UNameIndex Element;
        stream.read(reinterpret_cast<char*>(&Element), sizeof(Element));
        Text << "\tNames[" << i << "]:\n";
        Text << "\t\t" << FormatHEX(Element) << " -> " << Reader->IndexToName(Element) << std::endl;
        Names.push_back(Element);
    }
    return true;
}

bool UProperty::Deserialize()
{
    if(!UField::Deserialize())
        return false;
    Text << "UProperty:\n";
    uint32_t tmpVal;
    stream.read(reinterpret_cast<char*>(&tmpVal), sizeof(tmpVal));
    ArrayDim = tmpVal % (1 << 16);
    ElementSize = tmpVal >> 16;
    Text << "\tArrayDim = " << FormatHEX(ArrayDim) << " (" << ArrayDim << ")" << std::endl;
    Text << "\tElementSize = " << FormatHEX(ElementSize) << " (" << ElementSize << ")" << std::endl;
    FlagsOffset = stream.tellg();
    stream.read(reinterpret_cast<char*>(&PropertyFlagsL), sizeof(PropertyFlagsL));
    Text << "\tPropertyFlagsL = " << FormatHEX(PropertyFlagsL) << std::endl;
    Text << FormatPropertyFlagsL(PropertyFlagsL);
    stream.read(reinterpret_cast<char*>(&PropertyFlagsH), sizeof(PropertyFlagsH));
    Text << "\tPropertyFlagsH = " << FormatHEX(PropertyFlagsH) << std::endl;
    Text << FormatPropertyFlagsH(PropertyFlagsH);
    stream.read(reinterpret_cast<char*>(&CategoryIndex), sizeof(CategoryIndex));
    Text << "\tCategoryIndex = " << FormatHEX(CategoryIndex) << " -> " << Reader->IndexToName(CategoryIndex) << std::endl;
    stream.read(reinterpret_cast<char*>(&ArrayEnumRef), sizeof(ArrayEnumRef));
    Text << "\tArrayEnumRef = " << FormatHEX((uint32_t)ArrayEnumRef) << " -> " << Reader->ObjRefToName(ArrayEnumRef) << std::endl;
    if (PropertyFlagsL & (uint32_t)UPropertyFlagsL::Net)
    {
        stream.read(reinterpret_cast<char*>(&RepOffset), sizeof(RepOffset));
        Text << "\tRepOffset = " << FormatHEX(RepOffset) << std::endl;
    }
    return true;
}

bool UByteProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UByteProperty:\n";
    stream.read(reinterpret_cast<char*>(&EnumObjRef), sizeof(EnumObjRef));
    Text << "\tEnumObjRef = " << FormatHEX((uint32_t)EnumObjRef) << " -> " << Reader->ObjRefToName(EnumObjRef) << std::endl;
    return true;
}

bool UObjectProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UObjectProperty:\n";
    stream.read(reinterpret_cast<char*>(&OtherObjRef), sizeof(OtherObjRef));
    Text << "\tOtherObjRef = " << FormatHEX((uint32_t)OtherObjRef) << " -> " << Reader->ObjRefToName(OtherObjRef) << std::endl;
    return true;
}

bool UClassProperty::Deserialize()
{
    if (!UObjectProperty::Deserialize())
        return false;
    Text << "UClassProperty:\n";
    stream.read(reinterpret_cast<char*>(&ClassObjRef), sizeof(ClassObjRef));
    Text << "\tClassObjRef = " << FormatHEX((uint32_t)ClassObjRef) << " -> " << Reader->ObjRefToName(ClassObjRef) << std::endl;
    return true;
}

bool UStructProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UStructProperty:\n";
    stream.read(reinterpret_cast<char*>(&StructObjRef), sizeof(StructObjRef));
    Text << "\tStructObjRef = " << FormatHEX((uint32_t)StructObjRef) << " -> " << Reader->ObjRefToName(StructObjRef) << std::endl;
    return true;
}

bool UFixedArrayProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UFixedArrayProperty:\n";
    stream.read(reinterpret_cast<char*>(&InnerObjRef), sizeof(InnerObjRef));
    Text << "\tInnerObjRef = " << FormatHEX((uint32_t)InnerObjRef) << " -> " << Reader->ObjRefToName(InnerObjRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&Count), sizeof(Count));
    Text << "\tCount = " << FormatHEX(Count) << " (" << Count << ")" << std::endl;
    return true;
}

bool UArrayProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UArrayProperty:\n";
    stream.read(reinterpret_cast<char*>(&InnerObjRef), sizeof(InnerObjRef));
    Text << "\tInnerObjRef = " << FormatHEX((uint32_t)InnerObjRef) << " -> " << Reader->ObjRefToName(InnerObjRef) << std::endl;
    return true;
}

bool UDelegateProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UDelegateProperty:\n";
    stream.read(reinterpret_cast<char*>(&FunctionObjRef), sizeof(FunctionObjRef));
    Text << "\tFunctionObjRef = " << FormatHEX((uint32_t)FunctionObjRef) << " -> " << Reader->ObjRefToName(FunctionObjRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&DelegateObjRef), sizeof(DelegateObjRef));
    Text << "\tDelegateObjRef = " << FormatHEX((uint32_t)DelegateObjRef) << " -> " << Reader->ObjRefToName(DelegateObjRef) << std::endl;
    return true;
}

bool UInterfaceProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UInterfaceProperty:\n";
    stream.read(reinterpret_cast<char*>(&InterfaceObjRef), sizeof(InterfaceObjRef));
    Text << "\tInterfaceObjRef = " << FormatHEX((uint32_t)InterfaceObjRef) << " -> " << Reader->ObjRefToName(InterfaceObjRef) << std::endl;
    return true;
}

bool UMapProperty::Deserialize()
{
    if (!UProperty::Deserialize())
        return false;
    Text << "UMapProperty:\n";
    stream.read(reinterpret_cast<char*>(&KeyObjRef), sizeof(KeyObjRef));
    Text << "\tKeyObjRef = " << FormatHEX((uint32_t)KeyObjRef) << " -> " << Reader->ObjRefToName(KeyObjRef) << std::endl;
    stream.read(reinterpret_cast<char*>(&ValueObjRef), sizeof(ValueObjRef));
    Text << "\tValueObjRef = " << FormatHEX((uint32_t)ValueObjRef) << " -> " << Reader->ObjRefToName(ValueObjRef) << std::endl;
    return true;
}

bool ULevel::Deserialize()
{
    UObjectReference A;
    uint32_t NumActors;
    if (!UObject::Deserialize())
        return false;
    uint32_t pos = (unsigned)stream.tellg();
    Text << "ULevel:\n";
    stream.read(reinterpret_cast<char*>(&A), sizeof(A));
    Text << "\tLevel object: " << FormatHEX((uint32_t)A) << " -> " << Reader->ObjRefToName(A) << std::endl;
    stream.read(reinterpret_cast<char*>(&NumActors), sizeof(NumActors));
    Text << "\tNum actors: " << FormatHEX(NumActors) << " = " << NumActors << std::endl;
    stream.read(reinterpret_cast<char*>(&A), sizeof(A));
    Text << "\tWorldInfo object: " << FormatHEX((uint32_t)A) << " -> " << Reader->ObjRefToName(A) << std::endl;
    Text << "\tActors:\n";
    for (unsigned i = 0; i < NumActors; ++i)
    {
        stream.read(reinterpret_cast<char*>(&A), sizeof(A));
        Actors.push_back(A);
        Text << "\t\t" << FormatHEX((char*)&A, sizeof(A)) << "\t//\t" << FormatHEX((uint32_t)A) << " -> " << Reader->ObjRefToName(A) << std::endl;
    }
    pos = (unsigned)stream.tellg();
    Text << "Stream relative position (debug info): " << FormatHEX(pos) << " (" << pos << ")\n";
    Text << "Object unknown, can't deserialize!\n";
    return true;
}

bool UObjectUnknown::Deserialize()
{
    /// to be on a safe side: don't deserialize unknown objects
    if (TryUnsafe == true)
    {
        if (!UObject::Deserialize())
            return false;
        uint32_t pos = (unsigned)stream.tellg();
        if (pos != Reader->GetExportEntry(Index).SerialSize)
        {
            Text << "Stream relative position (debug info): " << FormatHEX(pos) << " (" << pos << ")\n";
        }
    }
    if ((unsigned)stream.tellg() != Reader->GetExportEntry(Index).SerialSize)
    {
        Text << "UObjectUnknown:\n";
        Text << "\tObject unknown, can't deserialize!\n";
    }
    return true;
}
