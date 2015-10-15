#include "UDefaultProperty.h"

#include "UObject.h"
#include "UPKReader.h"
#include "TextUtils.h"
#include <cstring>

bool UDefaultPropertiesList::Deserialize()
{
    Owner->Text << "UDefaultPropertiesList:\n";
    PropertyOffset = Owner->stream.tellg();
    DefaultProperties.clear();
    UDefaultProperty Property;
    do
    {
        Property = UDefaultProperty{};
        Property.Init(Owner, Owner->TryUnsafe, Owner->QuickMode);
        if (!Property.Deserialize())
        {
            _LogError("Failed to deserialize property " + Property.Name +
                      "\n\tProperty owner: " + Owner->Reader->GetExportEntry(Owner->Index).FullName, "UDefaultPropertiesList");
            Owner->Text << "Error deserializing property!\n";
            return false;
        }
        _LogDebug("Deserialized property " + Property.Name, "UDefaultPropertiesList");
        DefaultProperties.push_back(Property);
    } while (Property.GetName() != "None" && Owner->stream.good());
    PropertySize = (unsigned)Owner->stream.tellg() - (unsigned)PropertyOffset;
    return true;
}

bool UDefaultProperty::Deserialize()
{
    Owner->Text << "UDefaultProperty:\n";
    Owner->stream.read(reinterpret_cast<char*>(&NameIdx), sizeof(NameIdx));
    Name = Owner->Reader->IndexToName(NameIdx);
    Owner->Text << "\tNameIdx: " << FormatHEX(NameIdx) << " -> " << Name << std::endl;
    if (Name != "None")
    {
        Owner->stream.read(reinterpret_cast<char*>(&TypeIdx), sizeof(TypeIdx));
        Type = Owner->Reader->IndexToName(TypeIdx);
        Owner->Text << "\tTypeIdx: " << FormatHEX(TypeIdx) << " -> " << Type << std::endl;
        Owner->stream.read(reinterpret_cast<char*>(&PropertySize), sizeof(PropertySize));
        Owner->Text << "\tPropertySize: " << FormatHEX(PropertySize) << std::endl;
        if (PropertySize > Owner->Reader->GetExportEntry(Owner->Index).SerialSize)
        {
            _LogError("Bad PropertySize!", "UDefaultProperty");
            return false;
        }
        Owner->stream.read(reinterpret_cast<char*>(&ArrayIdx), sizeof(ArrayIdx));
        Owner->Text << "\tArrayIdx: " << FormatHEX(ArrayIdx) << std::endl;
        if (Type == "BoolProperty")
        {
            Owner->stream.read(reinterpret_cast<char*>(&BoolValue), sizeof(BoolValue));
            Owner->Text << "\tBoolean value: " << FormatHEX(BoolValue) << " = ";
            if (BoolValue == 0)
                Owner->Text << "false\n";
            else
                Owner->Text << "true\n";
        }
        if (Type == "StructProperty" || Type == "ByteProperty")
        {
            Owner->stream.read(reinterpret_cast<char*>(&InnerNameIdx), sizeof(InnerNameIdx));
            Owner->Text << "\tInnerNameIdx: " << FormatHEX(InnerNameIdx) << " -> " << Owner->Reader->IndexToName(InnerNameIdx) << std::endl;
            if (Type == "StructProperty")
                Type = Owner->Reader->IndexToName(InnerNameIdx);
            if (Type == "ByteProperty" && PropertySize == 8)
                Type == "NameProperty";
        }
        if (PropertySize > 0)
        {
            size_t offset = Owner->stream.tellg();
            if (QuickMode == false)
            {
                DeserializeValue();
            }
            else
            {
                _LogDebug("Quick mode: skipping default property deserialization.", "UDefaultProperty");
                Owner->Text << "Quick mode: skipping value.\n";
            }
            /// skip property value or fix stream pos after possible deserialization errors
            Owner->stream.seekg(offset + PropertySize);
        }
    }
    return true;
}

bool UDefaultProperty::DeserializeValue()
{
    if (Type == "ArrayProperty")
    {
        uint32_t NumElements;
        Owner->stream.read(reinterpret_cast<char*>(&NumElements), sizeof(NumElements));
        Owner->Text << "\tNumElements = " << FormatHEX(NumElements) << " = " << NumElements << std::endl;
        if (NumElements > PropertySize)
        {
            _LogError("Bad NumElements!", "UDefaultProperty");
            return false;
        }
        if ((NumElements > 0) && (PropertySize > 4))
        {
            std::string ArrayInnerType = FindArrayType();
            Owner->Text << "\tArrayInnerType = " << ArrayInnerType << std::endl;
            if (ArrayInnerType == "None" && TryUnsafe == true)
            {
                ArrayInnerType = GuessArrayType();
                if (ArrayInnerType != "None")
                    Owner->Text << "\tUnsafe guess: ArrayInnerType = " << ArrayInnerType << std::endl;
            }
            UDefaultProperty InnerProperty;
            InnerProperty.Name = Type;
            InnerProperty.Init(Owner, Owner->TryUnsafe, Owner->QuickMode);
            InnerProperty.Type = ArrayInnerType;
            InnerProperty.PropertySize = PropertySize - 4;
            if (ArrayInnerType != "None")
            {
                InnerProperty.PropertySize /= NumElements;
                for (unsigned i = 0; i < NumElements; ++i)
                {
                    Owner->Text << "\t" << Name << "[" << i << "]:\n";
                    InnerProperty.DeserializeValue();
                }
            }
            else
            {
                bool EndsWithNone = false;
                /// check if there is an inner property list
                if (InnerProperty.PropertySize > 8 && TryUnsafe == true)
                {
                    _LogDebug("Attempting to determine inner array type.", "UDefaultProperty");
                    size_t offset = Owner->stream.tellg();
                    std::vector<char> IPD(InnerProperty.PropertySize);
                    Owner->stream.read(IPD.data(), IPD.size());
                    UNameIndex NI;
                    memcpy((char*)&NI, IPD.data() + IPD.size() - 8, 8);
                    Owner->stream.seekg(offset);
                    EndsWithNone = Owner->Reader->IsNoneIdx(NI);
                }
                /// something that ends with 'None' is probably a list of properties
                if (EndsWithNone && TryUnsafe == true)
                {
                    _LogDebug("Unsafe guess: it's a Property List.", "UDefaultProperty");
                    for (unsigned i = 0; i < NumElements; ++i)
                    {
                        Owner->Text << "\t" << Name << "[" << i << "]:\n";
                        Owner->Text << "Unsafe guess (it's a Property List):\n";
                        UDefaultPropertiesList SomeProperties;
                        SomeProperties.Init(Owner);
                        SomeProperties.Deserialize();
                    }
                }
                else if (TryUnsafe == true)
                {
                    _LogDebug("Unsafe guess: it's a uniform array.", "UDefaultProperty");
                    InnerProperty.PropertySize /= NumElements;
                    for (unsigned i = 0; i < NumElements; ++i)
                    {
                        Owner->Text << "\t" << Name << "[" << i << "]:\n";
                        InnerProperty.DeserializeValue();
                    }
                }
                else
                {
                    _LogDebug("Unknown property type, deserializing as a single value.", "UDefaultProperty");
                    InnerProperty.DeserializeValue();
                }
            }
        }
    }
    else if (Type == "BoolProperty")
    {
        uint8_t boolVal;
        Owner->stream.read(reinterpret_cast<char*>(&boolVal), sizeof(boolVal));
        Owner->Text << "\tBoolean value: " << FormatHEX(boolVal) << " = ";
        if (boolVal == 0)
            Owner->Text << "false\n";
        else
            Owner->Text << "true\n";
    }
    else if (Type == "ByteProperty")
    {
        uint8_t byteVal;
        Owner->stream.read(reinterpret_cast<char*>(&byteVal), sizeof(byteVal));
        Owner->Text << "\tBoolean value: " << FormatHEX(byteVal) << " = " << (int)byteVal << "\n";
    }
    else if (Type == "IntProperty")
    {
        int32_t value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tInteger: " << FormatHEX((uint32_t)value) << " = " << value << std::endl;
    }
    else if (Type == "FloatProperty")
    {
        float value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tFloat: " << FormatHEX(value) << " = " << value << std::endl;
    }
    else if (Type == "ObjectProperty" ||
             Type == "InterfaceProperty" ||
             Type == "ComponentProperty" ||
             Type == "ClassProperty")
    {
        UObjectReference value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tObject: " << FormatHEX((uint32_t)value) << " = ";
        if (value == 0)
            Owner->Text << "none\n";
        else
            Owner->Text << Owner->Reader->ObjRefToName(value) << std::endl;
    }
    else if (Type == "DelegateProperty")
    {
        UObjectReference value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tReturn Value (?): " << FormatHEX((uint32_t)value) << " = ";
        Owner->Text << Owner->Reader->ObjRefToName(value) << std::endl;
        UNameIndex value2;
        Owner->stream.read(reinterpret_cast<char*>(&value2), sizeof(value2));
        Owner->Text << "\tDelegate Name: " << FormatHEX(value2) << " = " << Owner->Reader->IndexToName(value2) << std::endl;
    }
    else if (Type == "NameProperty")
    {
        UNameIndex value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tName: " << FormatHEX(value) << " = " << Owner->Reader->IndexToName(value) << std::endl;
    }
    else if (Type == "StrProperty")
    {
        int32_t StrLength;
        Owner->stream.read(reinterpret_cast<char*>(&StrLength), sizeof(StrLength));
        Owner->Text << "\tStrLength = " << FormatHEX((uint32_t)StrLength) << " = " << StrLength << std::endl;
        if (StrLength > 0)
        {
            std::string str;
            getline(Owner->stream, str, '\0');
            Owner->Text << "\tString = " << str << std::endl;
        }
        else if (StrLength < 0)
        {
            /// hacky unicode string reading
            StrLength = -StrLength * 2;
            std::string uStr;
            for (int i = 0; i < StrLength; ++i)
            {
                char ch = Owner->stream.get();
                if (i%2 == 0)
                    uStr += ch;
            }
            Owner->Text << "\tUnicode String = " << uStr << std::endl;
        }
    }
    else if (Type == "Vector")
    {
        float X, Y, Z;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->Text << "\tVector (X, Y, Z) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ") = ("
           << X << ", " << Y << ", " << Z << ")" << std::endl;
    }
    else if (Type == "Plane")
    {
        float X, Y, Z, W;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->stream.read(reinterpret_cast<char*>(&W), sizeof(W));
        Owner->Text << "\tPlane (X, Y, Z, W) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ", " << FormatHEX(W) << ") = ("
           << X << ", " << Y << ", " << Z << ", " << W << ")" << std::endl;
    }
    else if (Type == "Rotator")
    {
        int32_t P, Y, R;
        Owner->stream.read(reinterpret_cast<char*>(&P), sizeof(P));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&R), sizeof(R));
        Owner->Text << "\tRotator (Pitch, Yaw, Roll) = ("
           << FormatHEX((uint32_t)P) << ", " << FormatHEX((uint32_t)Y) << ", " << FormatHEX((uint32_t)R) << ") = ("
           << P << ", " << Y << ", " << R << ")" << std::endl;
    }
    else if (Type == "Vector2D")
    {
        float X, Y;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->Text << "\tVector2D (X, Y) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ") = ("
           << X << ", " << Y << ")" << std::endl;
    }
    else if (Type == "Guid")
    {
        FGuid GUID;
        Owner->stream.read(reinterpret_cast<char*>(&GUID), sizeof(GUID));
        Owner->Text << "\tGUID = " << FormatHEX(GUID) << std::endl;
    }
    else if (Type == "Color")
    {
        uint8_t R, G, B, A;
        Owner->stream.read(reinterpret_cast<char*>(&R), sizeof(R));
        Owner->stream.read(reinterpret_cast<char*>(&G), sizeof(G));
        Owner->stream.read(reinterpret_cast<char*>(&B), sizeof(B));
        Owner->stream.read(reinterpret_cast<char*>(&A), sizeof(A));
        Owner->Text << "\tColor (R, G, B, A) = ("
           << FormatHEX(R) << ", " << FormatHEX(G) << ", " << FormatHEX(B) << ", " << FormatHEX(A) << ") = ("
           << (unsigned)R << ", " << (unsigned)G << ", " << (unsigned)B << ", " << (unsigned)A << ")" << std::endl;
    }
    else if (Type == "LinearColor")
    {
        float R, G, B, A;
        Owner->stream.read(reinterpret_cast<char*>(&R), sizeof(R));
        Owner->stream.read(reinterpret_cast<char*>(&G), sizeof(G));
        Owner->stream.read(reinterpret_cast<char*>(&B), sizeof(B));
        Owner->stream.read(reinterpret_cast<char*>(&A), sizeof(A));
        Owner->Text << "\tLinearColor (R, G, B, A) = ("
           << FormatHEX(R) << ", " << FormatHEX(G) << ", " << FormatHEX(B) << ", " << FormatHEX(A) << ") = ("
           << R << ", " << G << ", " << B << ", " << A << ")" << std::endl;
    }
    else if (Type == "Box")
    {
        float X, Y, Z;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->Text << "\tVector Min (X, Y, Z) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ") = ("
           << X << ", " << Y << ", " << Z << ")" << std::endl;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->Text << "\tVector Max (X, Y, Z) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ") = ("
           << X << ", " << Y << ", " << Z << ")" << std::endl;
        uint8_t byteVal = 0;
        Owner->stream.read(reinterpret_cast<char*>(&byteVal), sizeof(byteVal));
        Owner->Text << "\tIsValid: " << FormatHEX(byteVal) << " = ";
        if (byteVal == 0)
            Owner->Text << "false\n";
        else
            Owner->Text << "true\n";
    }
    else if (Type == "Matrix")
    {
        float X, Y, Z, W;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->stream.read(reinterpret_cast<char*>(&W), sizeof(W));
        Owner->Text << "\tXPlane (X, Y, Z, W) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ", " << FormatHEX(W) << ") = ("
           << X << ", " << Y << ", " << Z << ", " << W << ")" << std::endl;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->stream.read(reinterpret_cast<char*>(&W), sizeof(W));
        Owner->Text << "\tYPlane (X, Y, Z, W) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ", " << FormatHEX(W) << ") = ("
           << X << ", " << Y << ", " << Z << ", " << W << ")" << std::endl;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->stream.read(reinterpret_cast<char*>(&W), sizeof(W));
        Owner->Text << "\tZPlane (X, Y, Z, W) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ", " << FormatHEX(W) << ") = ("
           << X << ", " << Y << ", " << Z << ", " << W << ")" << std::endl;
        Owner->stream.read(reinterpret_cast<char*>(&X), sizeof(X));
        Owner->stream.read(reinterpret_cast<char*>(&Y), sizeof(Y));
        Owner->stream.read(reinterpret_cast<char*>(&Z), sizeof(Z));
        Owner->stream.read(reinterpret_cast<char*>(&W), sizeof(W));
        Owner->Text << "\tWPlane (X, Y, Z, W) = ("
           << FormatHEX(X) << ", " << FormatHEX(Y) << ", " << FormatHEX(Z) << ", " << FormatHEX(W) << ") = ("
           << X << ", " << Y << ", " << Z << ", " << W << ")" << std::endl;
    }
    else if (Type == "ScriptStruct")
    {
        UDefaultPropertiesList SomeProperties;
        SomeProperties.Init(Owner);
        SomeProperties.Deserialize();
    }
    /// if it is big, it might be inner property list
    /// avoid this assumption if already inside an ArrayProperty!
    else if(TryUnsafe == true && Name != "ArrayProperty" && PropertySize > 24)
    {
        _LogDebug("Unsafe guess: it's a Property List.", "UDefaultProperty");
        Owner->Text << "Unsafe guess (it's a Property List):\n";
        UDefaultPropertiesList SomeProperties;
        SomeProperties.Init(Owner);
        SomeProperties.Deserialize();
    }
    /// Guid?
    else if(TryUnsafe == true && PropertySize == 16)
    {
        _LogDebug("Unsafe guess: it's GUID.", "UDefaultProperty");
        FGuid GUID;
        Owner->stream.read(reinterpret_cast<char*>(&GUID), sizeof(GUID));
        Owner->Text << "\tUnsafe guess: GUID = " << FormatHEX(GUID) << std::endl;
    }
    /// if it is small, it might be NameIndex
    else if(TryUnsafe == true && PropertySize == 8)
    {
        _LogDebug("Unsafe guess: it's a NameIndex.", "UDefaultProperty");
        UNameIndex value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tUnsafe guess:\n";
        Owner->Text << "\tName: " << FormatHEX(value) << " = " << Owner->Reader->IndexToName(value) << std::endl;
    }
    /// if it is even smaller, it might be an integer (or a float) or an object reference
    else if(TryUnsafe == true && PropertySize == 4)
    {
        _LogDebug("Unsafe guess: it's an Integer or a Reference.", "UDefaultProperty");
        int32_t value;
        Owner->stream.read(reinterpret_cast<char*>(&value), sizeof(value));
        Owner->Text << "\tUnsafe guess: "
           << "It's an Integer: " << FormatHEX((uint32_t)value) << " = " << value
           << " or a Reference: " << FormatHEX((uint32_t)value) << " -> " << Owner->Reader->ObjRefToName(value) << std::endl;
    }
    /// it can even be a boolean
    else if (TryUnsafe == true && PropertySize == 1)
    {
        _LogDebug("Unsafe guess: it's a boolean.", "UDefaultProperty");
        uint8_t boolVal;
        Owner->stream.read(reinterpret_cast<char*>(&boolVal), sizeof(boolVal));
        Owner->Text << "\tUnsafe guess: It's a boolean: " << FormatHEX(boolVal) << " = ";
        if (boolVal == 0)
            Owner->Text << "false\n";
        else
            Owner->Text << "true\n";
    }
    else
    {
        _LogDebug("Unknown property, skipping.", "UDefaultProperty");
        if (PropertySize <= Owner->Reader->GetExportEntry(Owner->Index).SerialSize)
        {
            std::vector<char> unk(PropertySize);
            Owner->stream.read(unk.data(), unk.size());
            Owner->Text << "\tUnknown property: " << FormatHEX(unk) << std::endl;
        }
    }
    return true;
}

std::string UDefaultProperty::FindArrayType()
{
    if (Owner->Index == 0)
        return "None";
    std::string OwnerName = Owner->Reader->GetEntryFullName(Owner->Index);
    size_t pos = OwnerName.find("Default__");
    if (pos == 0)
    {
        OwnerName = OwnerName.substr(9);
    }
    std::string FullName = OwnerName + "." + Name;
    UObjectReference ObjRef = Owner->Reader->FindObject(FullName);
    if (ObjRef == 0)
    {
        FullName = Owner->Reader->GetEntryType(Owner->Index) + "." + Name;
        ObjRef = Owner->Reader->FindObject(FullName);
    }
    if (ObjRef > 0)
    {
        if (Owner->Reader->GetEntryType(ObjRef) == "ArrayProperty")
        {
            UObjectReference InnerRef = Owner->Reader->GetExportObject(ObjRef)->GetInner();
            if (Owner->Reader->GetEntryType(InnerRef) == "StructProperty")
            {
                InnerRef = Owner->Reader->GetExportObject(InnerRef)->GetStructObjRef();
            }
            return Owner->Reader->GetEntryType(InnerRef);
        }
    }
    return "None";
}

std::string UDefaultProperty::GuessArrayType()
{
    if (Name == "VertexData")
        return "Vector";
    if (Name == "PermutedVertexData")
        return "Plane";
    if (Name == "FaceTriData")
        return "IntProperty";
    if (Name == "EdgeDirections")
        return "Vector";
    if (Name == "FaceNormalDirections")
        return "Vector";
    if (Name == "FacePlaneData")
        return "Plane";
    if (Name == "ElemBox")
        return "Box";
    return "None";
}
