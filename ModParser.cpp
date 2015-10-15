#include "ModParser.h"
#include "TextUtils.h"

std::string ModParser::GetText()
{
    std::string str = "";
    std::string line = "";
    size_t savedPos = modFile.tellg();
    line = GetLine(); /// read key/section line
    if (isKey)
    {
        str += line.substr(line.find("=") + 1);
        while ( modFile.good() )
        {
            savedPos = modFile.tellg();
            line = GetLine();
            if (FindKey(line) != -1 || FindSection(line) != -1)
            {
                modFile.clear(); /// to parse last line correctly
                break;
            }
            str += std::string("\n") + line;
        }
    }
    else if (isSection)
    {
        while ( modFile.good() )
        {
            savedPos = modFile.tellg();
            line = GetLine();
            if (FindKey(line) != -1 || FindSection(line) != -1)
            {
                modFile.clear(); /// to parse last line correctly
                break;
            }
            if (str != "") str += "\n";
            str += line;
        }
    }
    modFile.seekg(savedPos, std::ios::beg);
    return str;
}

std::string ModParser::GetTextValue()
{
    return Value;
}

std::vector<char> ModParser::GetDataChunk()
{
    return ::GetDataChunk(Value);
}

std::string ModParser::GetStringValue()
{
    return ::GetStringValue(Value);
}

int ModParser::GetIntValue()
{
    return ::GetIntValue(Value);
}

float ModParser::GetFloatValue()
{
    return ::GetFloatValue(Value);
}

std::string ModParser::GetLine()
{
    std::string line = "";
    int ch = 0;
    while(ch != 0x0D && ch != 0x0A && modFile.good())
    {
        ch = modFile.get();
        if (modFile.fail() || modFile.bad())
            break;
        if (CStyleComments && ch == '/' && modFile.peek() == '/')
        {
            while (ch != 0x0D && ch != 0x0A && modFile.good())
                ch = modFile.get();
        }
        else if (CStyleComments && ch == '/' && modFile.peek() == '*')
        {
            while (modFile.good())
            {
                ch = modFile.get();
                if (ch == '*' && modFile.peek() == '/')
                {
                    ch = modFile.get();
                    break;
                }
            }
        }
        else if (ch == commentLine)
        {
            while (ch != 0x0D && ch != 0x0A && modFile.good())
                ch = modFile.get();
        }
        else if (ch == commentBegin)
        {
            while (ch != commentEnd && modFile.good())
                ch = modFile.get();
        }
        else if (ch != 0x0D && ch != 0x0A && modFile.good())
        {
            line += ch;
        }
    }
    if (modFile.peek() == 0x0A || modFile.peek() == 0x0D)
        modFile.get();
    return line;
}

int ModParser::FindNext()
{
    if (!modFile.good())
        return -1;
    Name = "";
    Value = "";
    Index = -1;
    int idx = -1, keyIdx = -1, sectionIdx = -1;
    std::string line = "";
    size_t savedPos = modFile.tellg();
    while ( keyIdx == -1 && sectionIdx == -1 && modFile.good() )
    {
        savedPos = modFile.tellg();
        line = GetLine();
        keyIdx = FindKey(line);
        sectionIdx = FindSection(line);
    }
    modFile.clear(); /// to get last line value correctly
    modFile.seekg(savedPos, std::ios::beg);
    isKey = (keyIdx != -1);
    isSection = (sectionIdx != -1);
    if (keyIdx != -1)
    {
        idx = keyIdx;
        Name = keyNames[idx];
    }
    if (sectionIdx != -1)
    {
        idx = sectionIdx;
        Name = sectionNames[idx];
    }
    if (idx != -1)
    {
        Value = GetText();
    }
    Index = idx;
    return idx;
}

int ModParser::FindKey(std::string str)
{
    size_t pos = str.find("=");
    if (pos == std::string::npos)
        return -1;
    std::string name = Trim(str.substr(0, pos));
    /*std::string name = str.substr(0, pos);
    name = name.substr(name.find_first_not_of(" "));
    name = name.substr(0, name.find_last_not_of(" ") + 1);*/
    int idx = FindKeyNameIdx(name);
    return idx;
}

int ModParser::FindSection(std::string str)
{
    if (str.find("[") == std::string::npos || str.find("]") == std::string::npos)
        return -1;
    std::string name = Trim(str);
    /*std::string name = str;
    name = name.substr(name.find_first_not_of(" "));
    name = name.substr(0, name.find_last_not_of(" ") + 1);*/
    int idx = FindSectionNameIdx(name);
    return idx;
}

int ModParser::FindKeyNameIdx(std::string name)
{
    int idx = -1;
    for (unsigned int i = 0; i < keyNames.size(); ++i)
        if (keyNames[i] == name)
            idx = i;
    return idx;
}

int ModParser::FindSectionNameIdx(std::string name)
{
    int idx = -1;
    for (unsigned int i = 0; i < sectionNames.size(); ++i)
        if (sectionNames[i] == name)
            idx = i;
    return idx;
}

bool ModParser::OpenModFile(const char* name)
{
    if (modFile.is_open())
    {
        modFile.close();
        modFile.clear();
    }
    modFile.open(name, std::ios::binary);
    if (!modFile.good())
        return false;
    /// check if file is text
    while (modFile.good())
    {
        char ch = modFile.get();
        if (modFile.eof())
            break;
        if (ch < 1 || ch > 127)
        {
            modFile.close();
            return false;
        }
    }
    modFile.clear();
    modFile.seekg(0);
    isKey = false;
    isSection = false;
    Name = "";
    Value = "";
    Index = -1;
    return true;
}

void ModParser::AddKeyName(std::string name)
{
    keyNames.push_back(name);
}

void ModParser::AddSectionName(std::string name)
{
    sectionNames.push_back(name);
}

void ModParser::SetKeyNames(std::vector<std::string> names)
{
    keyNames = names;
}

void ModParser::SetSectionNames(std::vector<std::string> names)
{
    sectionNames = names;
}

void ModParser::SetCommentMarkers(char begMarker, char endMarker, char lineMarker)
{
    commentBegin = begMarker;
    commentEnd = endMarker;
    commentLine = lineMarker;
}
