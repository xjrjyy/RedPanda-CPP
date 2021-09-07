#include "project.h"
#include "editor.h"
#include "mainwindow.h"
#include "utils.h"
#include "systemconsts.h"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

Project::Project(QObject *parent) : QObject(parent)
{

}

QString Project::directory()
{
    QFileInfo fileInfo(mFilename);
    return fileInfo.absolutePath();
}

QString Project::executableName()
{
    QString exeFileName;
    if (mOptions.overrideOutput && !mOptions.overridenOutput.isEmpty()) {
        exeFileName = mOptions.overridenOutput;
    } else {
        switch(mOptions.type) {
        case ProjectType::StaticLib:
            exeFileName = changeFileExt(baseFileName(mFilename),STATIC_LIB_EXT);
            break;
        case ProjectType::DynamicLib:
            exeFileName = changeFileExt(baseFileName(mFilename),DYNAMIC_LIB_EXT);
            break;
        default:
            exeFileName = changeFileExt(baseFileName(mFilename),EXECUTABLE_EXT);
        }
    }
    QString exePath;
    if (!mOptions.exeOutput.isEmpty()) {
        QDir baseDir(directory());
        exePath = baseDir.filePath(mOptions.exeOutput);
    } else {
        exePath = directory();
    }
    QDir exeDir(exePath);
    return exeDir.filePath(exeFileName);
}

QString Project::makeFileName()
{
    if (mOptions.useCustomMakefile)
        return mOptions.customMakefile;
    else
        return QDir(directory()).filePath(MAKEFILE_NAME);
}

bool Project::modified()
{
    // Project file modified? Done
    if (mModified)
        return true;// quick exit avoids loop over all units

    // Otherwise, check all units
    foreach (const PProjectUnit& unit, mUnits){
        if (unit->modified())
            return true;
    }
    return false;
}

void Project::open()
{
    QFile fileInfo(mFilename);
    if (fileInfo.exists()
            && !fileInfo.isWritable()) {
        if (QMessageBox::question(pMainWindow,
                                  tr("Remove Readonly Attribute"),
                                  tr("Project file '%1' is readonly.<br /> Remove the readonly attribute?")
                                  .arg(mFilename),
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::Yes) == QMessageBox::Yes) {
            fileInfo.setPermissions(
                        QFileDevice::WriteOwner
                        | QFileDevice::WriteGroup
                        | QFileDevice::WriteUser
                        );
        }
    }
    loadOptions();

    mNode = makeProjectNode();

    checkProjectFileForUpdate();

    mIniFile->beginGroup("Project");
    int uCount  = mIniFile->value("UnitCount",0).toInt();
    mIniFile->endGroup();
    //createFolderNodes;
    QDir dir(directory());
    for (int i=0;i<uCount;i++) {
        PProjectUnit newUnit = std::make_shared<ProjectUnit>();
        mIniFile->beginGroup(QString("Unit%1").arg(i));
        newUnit->setFileName(dir.filePath(mIniFile->value("FileName","").toString()));
        if (!QFileInfo(newUnit->fileName()).exists()) {
            QMessageBox::critical(pMainWindow,
                                  tr("File Not Found"),
                                  tr("Project file '%1' can't be found!")
                                  .arg(newUnit->fileName()),
                                  QMessageBox::Ok);
            newUnit->setModified(true);
        } else {
            newUnit->setFolder(mIniFile->value("Folder","").toString());
            newUnit->setCompile(mIniFile->value("Compile", true).toBool());
            newUnit->setCompileCpp(
                        mIniFile->value("CompileCpp",mOptions.useGPP).toBool());

            newUnit->setLink(mIniFile->value("Link", true).toBool());
            newUnit->setPriority(mIniFile->value("Priority", 1000).toInt());
            newUnit->setOverrideBuildCmd(mIniFile->value("OverrideBuildCmd", false).toInt());
            newUnit->setBuildCmd(mIniFile->value("BuildCmd", "").toString());
            newUnit->setDetectEncoding(mIniFile->value("DetectEncoding", mOptions.useUTF8).toBool());
            newUnit->setEncoding(mIniFile->value("Encoding",ENCODING_SYSTEM_DEFAULT).toByteArray());

            newUnit->setEditor(nullptr);
            newUnit->setNew(false);
            newUnit->setParent(this);
            newUnit->setNode(makeNewFileNode(baseFileName(newUnit->fileName()), false, folderNodeFromName(newUnit->folder())));
            newUnit->node()->unitIndex = mUnits.count();
            mUnits.append(newUnit);
        }
        mIniFile->endGroup();
    }
    rebuildNodes();
}

void Project::setFileName(const QString &value)
{
    if (mFilename!=value) {
        mIniFile->sync();
        mIniFile.reset();
        QFile::copy(mFilename,value);
        mFilename = value;
        setModified(true);
        mIniFile = std::make_shared<QSettings>(mFilename);
    }
}

void Project::setModified(bool value)
{
    QFile file(mFilename);
    // only mark modified if *not* read-only
    if (!file.exists()
            || (file.exists() && file.isWritable())) {
        mModified=value;
        emit modifyChanged(mModified);
    }
}

void Project::addFolder(const QString &s)
{
    if (mFolders.indexOf(s)<0) {
        mFolders.append(s);
        rebuildNodes();
        //todo: MainForm.ProjectView.Select(FolderNodeFromName(s));
        //folderNodeFromName(s)->makeVisible();
        setModified(true);
    }
}

PProjectUnit Project::addUnit(const QString &inFileName, PFolderNode parentNode, bool rebuild)
{
    PProjectUnit newUnit;
    // Don't add if it already exists
    if (fileAlreadyExists(inFileName)) {
        QMessageBox::critical(pMainWindow,
                                 tr("File Exists"),
                                 tr("File '%1' is already in the project"),
                              QMessageBox::Ok);
        return newUnit;
    }
    newUnit = std::make_shared<ProjectUnit>(this);

    // Set all properties
    newUnit->setFileName(QDir(directory()).filePath(inFileName));
    newUnit->setNew(false);
    newUnit->setEditor(nullptr);
    newUnit->setFolder(getFolderPath(parentNode));
    newUnit->setNode(makeNewFileNode(baseFileName(newUnit->fileName()), false, parentNode);
    newUnit->node()->unitIndex = mUnits.count();
    mUnits.append(newUnit);

  // Determine compilation flags
    switch(getFileType(inFileName)) {
    case FileType::CSource:
        newUnit->setCompile(true);
        newUnit->setCompileCpp(mOptions.useGPP);
        newUnit->setLink(true);
        break;
    case FileType::CppSource:
        newUnit->setCompile(true);
        newUnit->setCompileCpp(true);
        newUnit->setLink(true);
        break;
    case FileType::WindowsResourceSource:
        newUnit->setCompile(true);
        newUnit->setCompileCpp(mOptions.useGPP);
        newUnit->setLink(false);
        break;
    default:
        newUnit->setCompile(false);
        newUnit->setCompileCpp(false);
        newUnit->setLink(false);
    }
    newUnit->setPriority(1000);
    newUnit->setOverrideBuildCmd(false);
    newUnit->setBuildCmd("");
    if (rebuild)
        rebuildNodes();
    setModified(true);
    return newUnit;
}

void Project::buildPrivateResource(bool forceSave)
{
    int comp = 0;
    foreach (const PProjectUnit& unit,mUnits) {
        if (
                (getFileType(unit->fileName()) == FileType::WindowsResourceSource)
                && unit->compile() )
            comp++;
    }

    // if project has no other resources included
    // and does not have an icon
    // and does not include the XP style manifest
    // and does not include version info
    // then do not create a private resource file
    if ((comp == 0) &&
            (! mOptions.supportXPThemes)
            && (! mOptions.includeVersionInfo)
            && (mOptions.icon == "")) {
        mOptions.privateResource="";
        return;
    }

    // change private resource from <project_filename>.res
    // to <project_filename>_private.res
    //
    // in many cases (like in importing a MSVC project)
    // the project's resource file has already the
    // <project_filename>.res filename.
    QString rcFile;
    if (!mOptions.privateResource.isEmpty()) {
        rcFile = QDir(directory()).filePath(mOptions.privateResource);
        if (changeFileExt(rcFile, DEV_PROJECT_EXT) == mFilename)
            rcFile = changeFileExt(mFilename,QString("_private") + RC_EXT);
    } else
        rcFile = changeFileExt(mFilename,QString("_private") + RC_EXT);
    rcFile = extractRelativePath(mFilename, rcFile);
    rcFile.replace(' ','_');

    // don't run the private resource file and header if not modified,
    // unless ForceSave is true
    if (!forceSave
            && fileExists(rcFile)
            && fileExists(changeFileExt(rcFile, H_EXT))
            && !mModified)
        return;

    QStringList content;
    content.append("/* THIS FILE WILL BE OVERWRITTEN BY DEV-C++ */");
    content.append("/* DO NOT EDIT! */");
    content.append("");

    if (mOptions.includeVersionInfo) {
      content.append("#include <windows.h> // include for version info constants");
      content.append("");
    }

    foreach (const PProjectUnit& unit, mUnits) {
        if (
                (getFileType(unit->fileName()) == FileType::WindowsResourceSource)
                && unit->compile() )
            content.append("#include \"" +
                           genMakePath(
                               extractRelativePath(directory(), unit->fileName()),
                               false,
                               false) + "\"");
    }

    if (!mOptions.icon.isEmpty()) {
        content.append("");
        QString icon = QDir(directory()).absoluteFilePath(mOptions.icon);
        if (fileExists(icon)) {
            icon = extractRelativePath(mFilename, icon);
            icon.replace('\\', '/');
            content.append("A ICON \"" + icon + '"');
        } else
            mOptions.icon = "";
    }

    if (mOptions.supportXPThemes) {
      content.append("");
      content.append("//");
      content.append("// SUPPORT FOR WINDOWS XP THEMES:");
      content.append("// THIS WILL MAKE THE PROGRAM USE THE COMMON CONTROLS");
      content.append("// LIBRARY VERSION 6.0 (IF IT IS AVAILABLE)");
      content.append("//");
      if (!mOptions.exeOutput.isEmpty())
          content.append(
                    "1 24 \"" +
                       genMakePath2(
                           includeTrailingPathDelimiter(mOptions.exeOutput)
                           + baseFileName(executable()))
                + ".Manifest\"");
      else
          content.append("1 24 \"" + baseFileName(executable()) + ".Manifest\"");
    }

    if (mOptions.includeVersionInfo) {
        content.append("");
        content.append("//");
        content.append("// TO CHANGE VERSION INFORMATION, EDIT PROJECT OPTIONS...");
        content.append("//");
        content.append("1 VERSIONINFO");
        content.append("FILEVERSION " +
                       QString("%1,%2,%3,%4")
                       .arg(mOptions.versionInfo.major)
                       .arg(mOptions.versionInfo.minor)
                       .arg(mOptions.versionInfo.release)
                       .arg(mOptions.versionInfo.build));
        content.append("PRODUCTVERSION " +
                       QString("%1,%2,%3,%4")
                       .arg(mOptions.versionInfo.major)
                       .arg(mOptions.versionInfo.minor)
                       .arg(mOptions.versionInfo.release)
                       .arg(mOptions.versionInfo.build));
        switch(mOptions.type) {
        case ProjectType::GUI:
        case ProjectType::Console:
            content.append("FILETYPE VFT_APP");
            break;
        case ProjectType::StaticLib:
            content.append("FILETYPE VFT_STATIC_LIB");
            break;
        case ProjectType::DynamicLib:
            content.append("FILETYPE VFT_DLL");
            break;
        }
        content.append("{");
        content.append("  BLOCK \"StringFileInfo\"");
        content.append("  {");
        content.append("    BLOCK \"" +
                       QString("%1%2")
                       .arg(mOptions.versionInfo.languageID,4,16,QChar('0'))
                       .arg(mOptions.versionInfo.charsetID,4,16,QChar('0'))
                       + '"');
        content.append("    {");
        content.append("      VALUE \"CompanyName\", \""
                       + mOptions.versionInfo.companyName
                       + "\"");
        content.append("      VALUE \"FileVersion\", \""
                       + mOptions.versionInfo.fileVersion
                       + "\"");
        content.append("      VALUE \"FileDescription\", \""
                       + mOptions.versionInfo.fileDescription
                       + "\"");
        content.append("      VALUE \"InternalName\", \""
                       + mOptions.versionInfo.internalName
                       + "\"");
        content.append("      VALUE \"LegalCopyright\", \""
                       + mOptions.versionInfo.legalCopyright
                       + '"');
        content.append("      VALUE \"LegalTrademarks\", \""
                       + mOptions.versionInfo.legalTrademarks
                       + "\"");
        content.append("      VALUE \"OriginalFilename\", \""
                       + mOptions.versionInfo.originalFilename
                       + "\"");
        content.append("      VALUE \"ProductName\", \""
                       + mOptions.versionInfo.productName + "\"");
        content.append("      VALUE \"ProductVersion\", \""
                       + mOptions.versionInfo.productVersion + "\"");
        content.append("    }");
        content.append("  }");

        // additional block for windows 95->NT
        content.append("  BLOCK \"VarFileInfo\"");
        content.append("  {");
        content.append("    VALUE \"Translation\", " +
                       QString("0x%1, %2")
                       .arg(mOptions.versionInfo.languageID,4,16,QChar('0'))
                       .arg(mOptions.versionInfo.charsetID));
        content.append("  }");

        content.append("}");
    }

    rcFile = QDir(directory()).absoluteFilePath(rcFile);
    if (content.count() > 3) {
        StringsToFile(content,rcFile);
        mOptions.privateResource = extractRelativePath(directory(), rcFile);
    } else {
      if (fileExists(rcFile))
          QFile::remove(rcFile);
      QString resFile = changeFileExt(rcFile, RES_EXT);
      if (fileExists(resFile))
          QFile::remove(resFile);
      mOptions.privateResource = "";
    }
//    if fileExists(Res) then
//      FileSetDate(Res, DateTimeToFileDate(Now)); // fix the "Clock skew detected" warning ;)

    // create XP manifest
    if (mOptions.supportXPThemes) {
        QStringList content;
        content.append("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>");
        content.append("<assembly");
        content.append("  xmlns=\"urn:schemas-microsoft-com:asm.v1\"");
        content.append("  manifestVersion=\"1.0\">");
        content.append("<assemblyIdentity");
        QString name = mName;
        name.replace(' ','_');
        content.append("    name=\"DevCpp.Apps." + name + '\"');
        content.append("    processorArchitecture=\"*\"");
        content.append("    version=\"1.0.0.0\"");
        content.append("    type=\"win32\"/>");
        content.append("<description>" + name + "</description>");
        content.append("<dependency>");
        content.append("    <dependentAssembly>");
        content.append("        <assemblyIdentity");
        content.append("            type=\"win32\"");
        content.append("            name=\"Microsoft.Windows.Common-Controls\"");
        content.append("            version=\"6.0.0.0\"");
        content.append("            processorArchitecture=\"*\"");
        content.append("            publicKeyToken=\"6595b64144ccf1df\"");
        content.append("            language=\"*\"");
        content.append("        />");
        content.append("    </dependentAssembly>");
        content.append("</dependency>");
        content.append("</assembly>");
        StringsToFile(content,executable() + ".Manifest");
    } else if (fileExists(executable() + ".Manifest"))
        QFile::remove(executable() + ".Manifest");

    // create private header file
    QString hFile = changeFileExt(rcFile, H_EXT);
    content.clear();
    QString def = baseFileName(rcFile);
    def.replace(".","_");
    content.append("/* THIS FILE WILL BE OVERWRITTEN BY DEV-C++ */");
    content.append("/* DO NOT EDIT ! */");
    content.append("");
    content.append("#ifndef " + def);
    content.append("#define " + def);
    content.append("");
    content.append("/* VERSION DEFINITIONS */");
    content.append("#define VER_STRING\t" +
                   QString("\"%d.%d.%d.%d\"")
                   .arg(mOptions.versionInfo.major)
                   .arg(mOptions.versionInfo.minor)
                   .arg(mOptions.versionInfo.release)
                   .arg(mOptions.versionInfo.build));
    content.append(QString("#define VER_MAJOR\t%1").arg(mOptions.versionInfo.major));
    content.append(QString("#define VER_MINOR\t%1").arg(mOptions.versionInfo.minor));
    content.append(QString("#define VER_RELEASE\t").arg(mOptions.versionInfo.release));
    content.append(QString("#define VER_BUILD\t").arg(mOptions.versionInfo.build));
    content.append(QString("#define COMPANY_NAME\t\"%1\"")
                   .arg(mOptions.versionInfo.companyName));
    content.append(QString("#define FILE_VERSION\t\"%1\"")
                   .arg(mOptions.versionInfo.fileVersion));
    content.append(QString("#define FILE_DESCRIPTION\t\"%1\"")
                   .arg(mOptions.versionInfo.fileDescription));
    content.append(QString("#define INTERNAL_NAME\t\"%1\"")
                   .arg(mOptions.versionInfo.internalName));
    content.append(QString("#define LEGAL_COPYRIGHT\t\"%1\"")
                   .arg(mOptions.versionInfo.legalCopyright));
    content.append(QString("#define LEGAL_TRADEMARKS\t\"%1\"")
                   .arg(mOptions.versionInfo.legalTrademarks));
    content.append(QString("#define ORIGINAL_FILENAME\t\"%1\"")
                   .arg(mOptions.versionInfo.originalFilename));
    content.append(QString("#define PRODUCT_NAME\t\"%1\"")
                   .arg(mOptions.versionInfo.productName));
    content.append(QString("#define PRODUCT_VERSION\t\"%1\"")
                   .arg(mOptions.versionInfo.productVersion));
    content.append("");
    content.append("#endif /*" + def + "*/");
    StringsToFile(content,hFile);
}

void Project::sortUnitsByPriority()
{
    std::sort(mUnits.begin(),mUnits.end(),[](const PProjectUnit& u1, const PProjectUnit& u2)->bool{
        return (u1->priority()>u2->priority());
    });
}

ProjectUnit::ProjectUnit(Project* parent)
{
    mEditor = nullptr;
    mNode = nullptr;
    mParent = parent;
}

Project *ProjectUnit::parent() const
{
    return mParent;
}

void ProjectUnit::setParent(Project* newParent)
{
    mParent = newParent;
}

Editor *ProjectUnit::editor() const
{
    return mEditor;
}

void ProjectUnit::setEditor(Editor *newEditor)
{
    mEditor = newEditor;
}

const QString &ProjectUnit::fileName() const
{
    return mFileName;
}

void ProjectUnit::setFileName(const QString &newFileName)
{
    mFileName = newFileName;
}

bool ProjectUnit::isNew() const
{
    return mNew;
}

void ProjectUnit::setNew(bool newNew)
{
    mNew = newNew;
}

const QString &ProjectUnit::folder() const
{
    return mFolder;
}

void ProjectUnit::setFolder(const QString &newFolder)
{
    mFolder = newFolder;
}

bool ProjectUnit::compile() const
{
    return mCompile;
}

void ProjectUnit::setCompile(bool newCompile)
{
    mCompile = newCompile;
}

bool ProjectUnit::compileCpp() const
{
    return mCompileCpp;
}

void ProjectUnit::setCompileCpp(bool newCompileCpp)
{
    mCompileCpp = newCompileCpp;
}

bool ProjectUnit::overrideBuildCmd() const
{
    return mOverrideBuildCmd;
}

void ProjectUnit::setOverrideBuildCmd(bool newOverrideBuildCmd)
{
    mOverrideBuildCmd = newOverrideBuildCmd;
}

const QString &ProjectUnit::buildCmd() const
{
    return mBuildCmd;
}

void ProjectUnit::setBuildCmd(const QString &newBuildCmd)
{
    mBuildCmd = newBuildCmd;
}

bool ProjectUnit::link() const
{
    return mLink;
}

void ProjectUnit::setLink(bool newLink)
{
    mLink = newLink;
}

int ProjectUnit::priority() const
{
    return mPriority;
}

void ProjectUnit::setPriority(int newPriority)
{
    mPriority = newPriority;
}

bool ProjectUnit::detectEncoding() const
{
    return mDetectEncoding;
}

void ProjectUnit::setDetectEncoding(bool newDetectEncoding)
{
    mDetectEncoding = newDetectEncoding;
}

const QByteArray &ProjectUnit::encoding() const
{
    return mEncoding;
}

void ProjectUnit::setEncoding(const QByteArray &newEncoding)
{
    mEncoding = newEncoding;
}

bool ProjectUnit::modified() const
{
    if (mEditor) {
        return mEditor->modified();
    } else {
        return false;
    }
}

void ProjectUnit::setModified(bool value)
{
    // Mark the change in the coupled editor
    if (mEditor) {
        return mEditor->setModified(value);
    }

    // If modified is set to true, mark project as modified too
    if (value) {
        mParent->setModified(true);
    }
}

bool ProjectUnit::save()
{
    bool previous=pMainWindow->fileSystemWatcher()->blockSignals(true);
    auto action = finally([&previous](){
        pMainWindow->fileSystemWatcher()->blockSignals(previous);
    });
    bool result=true;
    if (!mEditor && !fileExists(mFileName)) {
        // file is neither open, nor saved
        QStringList temp;
        StringsToFile(temp,mFileName);
    } else if (mEditor and modified()) {
        result = mEditor->save();
    }
    if (mNode) {
        mNode->text = baseFileName(mFileName);
    }
    return result;
}

PFolderNode &ProjectUnit::node()
{
    return mNode;
}

void ProjectUnit::setNode(const PFolderNode &newNode)
{
    mNode = newNode;
}