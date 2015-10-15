#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include "UPKDeclarations.h"

std::string EatWhite(std::string str, char delim);

std::string Trim(std::string str);
size_t SplitAt(char ch, std::string in, std::string& out1, std::string& out2);
std::string GetFilename(std::string str);
std::string GetFilenameNoExt(std::string str);
std::string MakeTextBlock(char *data, size_t dataSize);
std::string GetStringValue(const std::string& TextBuffer);
std::vector<char> GetDataChunk(const std::string& TextBuffer);
int GetIntValue(const std::string& TextBuffer);
unsigned GetUnsignedValue(const std::string& TextBuffer);
float GetFloatValue(const std::string& TextBuffer);

std::string FormatHEX(uint32_t val);
std::string FormatHEX(uint16_t val);
std::string FormatHEX(uint8_t val);
std::string FormatHEX(float val);
std::string FormatHEX(FGuid GUID);
std::string FormatHEX(UNameIndex NameIndex);
std::string FormatHEX(uint32_t L, uint32_t H);
std::string FormatHEX(std::vector<char> DataChunk);
std::string FormatHEX(char* DataChunk, size_t size);
std::string FormatHEX(std::string DataString);

#endif // TEXTUTILS_H
