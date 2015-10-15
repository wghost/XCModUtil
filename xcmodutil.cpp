#include <iostream>
#include <cstdlib>

#include <wx/string.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/cmdline.h>
#include <wx/msgout.h>

#include "UPKReader.h"
#include "UPKExtractor.h"
#include "TextUtils.h"

/// wxWidgets stderr log
class wxMessageProgLog: public ProgLog
{
public:
    wxMessageProgLog()
    {
        wxMessageOutput::Set(&OutputStderr);
    }
    virtual void Log(const std::string& message, const std::string& sender = DEFAULT_SENDER, const ELogLevel& level = DEFAULT_LEVEL)
    {
        if (!IsLoggingAllowed(sender, level))
        {
            return;
        }
        wxMessageOutput::Get()->Output(FormatLogLevel(level) + FormatSender(sender) + message + "\n");
    }
private:
    wxMessageOutputStderr OutputStderr;
};

int main(int argN, char* argV[])
{
    /// init program log
    wxMessageProgLog MyProgLog;
    LogService::ProvideLog(&MyProgLog);
    /// static logo text
    static const std::string myLogo = "XCOM EU/EW Modding Utility by Wasteland Ghost aka wghost81";
    /// parse command line
    static const wxCmdLineEntryDesc cmdLineDesc[] =
    {
        { wxCMD_LINE_PARAM,  NULL, NULL,     "<input file>", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_SWITCH, "h", "help",    "get usage help (this text)", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_SWITCH, "v", "verbose", "be verbose" },
        { wxCMD_LINE_OPTION, "l", "loglevel","set log level", wxCMD_LINE_VAL_NUMBER },
        { wxCMD_LINE_SWITCH, "b", "backup",  "create backup" },
        { wxCMD_LINE_OPTION, "i", "input",   "set input dir" },
        { wxCMD_LINE_OPTION, "o", "output",  "set output dir" },
        { wxCMD_LINE_SWITCH, "d", "decompress", "save decompressed package" },
        { wxCMD_LINE_SWITCH, "t", "tables",  "extract tables" },
        { wxCMD_LINE_OPTION, "e", "entry",   "find entry by name", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_OPTION, "f", "offset",  "find entry by file offset", wxCMD_LINE_VAL_NUMBER },
        { wxCMD_LINE_SWITCH, "s", "serialized", "extract export entry serialized data" },
        { wxCMD_LINE_OPTION, "x", "extract", "extract objects with names matching to regular expression (use --extract=\".*\" to extract all objects)", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_OPTION, "c", "compare", "compare to other package", wxCMD_LINE_VAL_STRING },
        { wxCMD_LINE_SWITCH, "p", "pseudocode", "decompile export entry script bytecode to patcher pseudocode" },
        { wxCMD_LINE_NONE }
    };
    wxCmdLineParser cmdLineParser(cmdLineDesc, argN, argV);
    cmdLineParser.SetLogo(myLogo);
    cmdLineParser.SetSwitchChars("-");
    cmdLineParser.EnableLongOptions();
    if (cmdLineParser.Parse() != 0)
    {
        return 1;
    }
    /// set log level
    long logLevel = (long)ELogLevel::Error; /// default log level: errors only
    if (cmdLineParser.Found("loglevel", &logLevel))
    {
        if (logLevel < 0)
            logLevel = (long)ELogLevel::Error;
        if (logLevel > (long)ELogLevel::All)
            logLevel = (long)ELogLevel::All;
    }
    _SetLogLevel(logLevel);
    /// check verbose
    bool verbose = false;
    if (cmdLineParser.Found("verbose"))
    {
        verbose = true;
    }
    /// main program
    if (verbose)
    {
        std::cout << myLogo << std::endl;
    }
    /// handle command line params
    /// directories
    wxString inputDirName;
    if (!cmdLineParser.Found("input", &inputDirName))
    {
        inputDirName = ".";
    }
    inputDirName = wxFileName(inputDirName).GetFullPath();
    if (verbose)
    {
        std::cout << "Input dir name: " << inputDirName << std::endl;
    }
    wxString outputDirName;
    if (!cmdLineParser.Found("output", &outputDirName))
    {
        outputDirName = ".";
    }
    outputDirName = wxFileName(outputDirName).GetFullPath();
    if (!wxDirExists(outputDirName))
    {
        if (!wxMkdir(outputDirName))
        {
            _LogError("Output directory does not exist: " + outputDirName, "xcmodutil");
            return 1;
        }
    }
    if (verbose)
    {
        std::cout << "Output dir name: " << outputDirName << std::endl;
    }
    /// upk file name
    wxString upkFileName = wxFileName(inputDirName + "/" + cmdLineParser.GetParam()).GetFullPath();
    if (verbose)
    {
        std::cout << "Upk file full path: " << upkFileName << std::endl;
    }
    if (!wxFileExists(upkFileName))
    {
         _LogError("Cannot find upk file: " + upkFileName, "xcmodutil");
         return 1;
    }
    /// make backup if backup switch is set
    if (cmdLineParser.Found("backup"))
    {
        wxString backupFileName = wxFileName(outputDirName + "/" + GetFilename(upkFileName.ToStdString()) + ".bak").GetFullPath();
        if (!wxCopyFile(upkFileName, backupFileName))
        {
            _LogError("Error writing backup to: " + backupFileName, "xcmodutil");
            return 1;
        }
        if (verbose)
        {
            std::cout << "Backup saved to: " << backupFileName << std::endl;
        }
    }
    /// open the package
    if (verbose)
    {
        std::cout << "Reading package: " << upkFileName << std::endl;
    }
    UPKReader package(upkFileName.c_str());
    UPKReadErrors err = package.GetError();
    if (err != UPKReadErrors::NoErrors)
    {
        _LogError("Error reading package: " + upkFileName, "xcmodutil");
        return 1;
    }
    /// if decompress switch is set
    if (cmdLineParser.Found("decompress"))
    {
        wxString decomprName = wxFileName(outputDirName + "/" + GetFilename(upkFileName.ToStdString())).GetFullPath();
        package.SavePackage(decomprName.c_str());
        if (verbose == true)
        {
            std::cout << "Decompressed package saved to: " << decomprName << std::endl;
        }
    }
    /// if extract option is set
    wxString nameMask;
    if (cmdLineParser.Found("extract", &nameMask))
    {
        /// extract all objects to separate dirs and files
        wxString baseDirName;
        wxFileName::SplitPath(upkFileName, nullptr, nullptr, &baseDirName, nullptr);
        baseDirName = wxFileName(outputDirName + "/" + baseDirName).GetFullPath();
        UPKExtractor::ExtractPackageObjects(&package, baseDirName.ToStdString(), nameMask.ToStdString());
        if (verbose)
        {
            std::cout << "Package extracted to dir: " << outputDirName << std::endl;
        }
    }
    /// no need to save tables if extract option is set
    else if (cmdLineParser.Found("tables"))
    {
        wxString listFileName = wxFileName(outputDirName + "/" + GetFilenameNoExt(upkFileName.ToStdString()) + ".txt").GetFullPath();
        UPKExtractor::ExtractPackageHeader(&package, listFileName.ToStdString());
        if (verbose)
        {
            std::cout << "Tables extracted to: " << listFileName << std::endl;
        }
    }
    /// extract entry
    wxString entryFullName = "", entryFileName = "";
    UObjectReference objRef = 0;
    long fileOffset = 0;
    bool saveDeserialized = false;
    /// find entry by full name
    if (cmdLineParser.Found("entry", &entryFullName))
    {
        objRef = package.FindObject(entryFullName.ToStdString(), false);
        if (objRef == 0)
        {
            _LogError("Cannot find object entry by name: " + entryFullName, "xcmodutil");
            return 1;
        }
        if (verbose)
        {
            std::cout << "Entry found!\n";
        }
        saveDeserialized = true;
    }
    /// find entry by offset
    if (cmdLineParser.Found("offset", &fileOffset))
    {
        objRef = package.FindObjectByOffset(fileOffset);
        if (objRef <= 0)
        {
            _LogError("Cannot find object by specified offset!", "xcmodutil");
            return 1;
        }
        entryFullName = package.GetEntryFullName(objRef);
        if (verbose)
        {
            std::cout << "Entry found: " << entryFullName << std::endl;
        }
        saveDeserialized = true;
    }
    /// if need to deserialize entry data
    if (saveDeserialized)
    {
        wxString entryFileName = wxFileName(outputDirName + "/" + entryFullName + ".txt").GetFullPath();
        UPKExtractor::ExtractEntry(&package, objRef, entryFileName.ToStdString());
        if (verbose && wxFileExists(entryFileName))
        {
            std::cout << "Entry extracted to: " << entryFileName << std::endl;
        }
    }
    /// if need to extract binary entry data
    if (cmdLineParser.Found("serialized") && objRef > 0)
    {
        package.SaveExportData((uint32_t)objRef, outputDirName.ToStdString());
        if (verbose == true)
        {
            std::cout << "Entry binary data saved to dir: " << outputDirName << std::endl;
        }
    }
    wxString anotherFileName = "";
    /// compare to another package
    if (cmdLineParser.Found("compare", &anotherFileName))
    {
        if (verbose)
        {
            std::cout << "Reading package: " << anotherFileName << std::endl;
        }
        UPKReader anotherPackage(anotherFileName.c_str());
        UPKReadErrors err2 = anotherPackage.GetError();
        if (err2 != UPKReadErrors::NoErrors)
        {
            _LogError("Error reading package: " + anotherFileName, "xcmodutil");
            return 1;
        }
        wxString outputFileName = wxFileName(outputDirName + "/compare_log.txt").GetFullPath();
        if (verbose)
        {
            std::cout << "Comparing packages...\n";
        }
        UPKExtractor::ComparePackages(package, anotherPackage, outputFileName.ToStdString());
        if (verbose)
        {
            std::cout << "Comparison results saved to " << outputFileName << std::endl;
        }
    }
    return 0;
}
