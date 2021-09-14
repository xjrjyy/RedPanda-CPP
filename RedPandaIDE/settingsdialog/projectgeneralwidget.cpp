#include "projectgeneralwidget.h"
#include "ui_projectgeneralwidget.h"
#include "../project.h"
#include "../mainwindow.h"

#include <QIcon>
#include <QTextCodec>

ProjectGeneralWidget::ProjectGeneralWidget(const QString &name, const QString &group, QWidget *parent) :
    SettingsWidget(name,group,parent),
    ui(new Ui::ProjectGeneralWidget)
{
    ui->setupUi(this);
}

ProjectGeneralWidget::~ProjectGeneralWidget()
{
    delete ui;
}

void ProjectGeneralWidget::doLoad()
{
    std::shared_ptr<Project> project = pMainWindow->project();
    if (!project)
        return;
    ui->txtName->setText(project->name());
    ui->txtFileName->setText(project->filename());
    ui->txtOutputFile->setText(project->executable());

    int srcCount=0,headerCount=0,resCount=0,otherCount=0, totalCount=0;
    foreach (const PProjectUnit& unit, project->units()) {
        switch(getFileType(unit->fileName())) {
        case FileType::CSource:
        case FileType::CppSource:
            srcCount++;
            break;
        case FileType::CppHeader:
        case FileType::CHeader:
            headerCount++;
            break;
        case FileType::WindowsResourceSource:
            resCount++;
            break;
        default:
            otherCount++;
        }
        totalCount++;
    }
    ui->lblFiles->setText(tr("%1 files [ %2 sources, %3 headers, %4 resources, %5 other files ]")
                          .arg(totalCount).arg(srcCount).arg(headerCount)
                          .arg(resCount).arg(otherCount));

    ui->cbDefaultEncoding->addItem(ENCODING_AUTO_DETECT);
    ui->cbDefaultEncoding->addItem(ENCODING_SYSTEM_DEFAULT);
    ui->cbDefaultEncoding->addItem(ENCODING_UTF8);
    QList<QByteArray> codecNames;
    QSet<QByteArray> codecAlias;
    codecAlias.insert("system");
    codecAlias.insert("utf-8");

    foreach (const QByteArray& name, QTextCodec::availableCodecs()){
        QByteArray lname = name.toLower();
        if (lname.startsWith("cp"))
            continue;
        if (codecAlias.contains(lname))
            continue;
        codecNames.append(lname);
        QTextCodec* codec = QTextCodec::codecForName(name);
        if (codec) {
            foreach (const QByteArray& alias, codec->aliases()) {
                codecAlias.insert(alias.toLower());
            }
        }
    }
    std::sort(codecNames.begin(),codecNames.end());
    foreach (const QByteArray& name,codecNames) {
        ui->cbDefaultEncoding->addItem(name);
    }
    ui->cbDefaultEncoding->setCurrentText(project->options().encoding);

    ui->lstType->setCurrentRow( static_cast<int>(project->options().type));

    ui->cbDefaultCpp->setChecked(project->options().useGPP);
    ui->cbSupportXPTheme->setChecked(project->options().supportXPThemes);
    mIconPath = project->options().icon;
    if (!mIconPath.isEmpty()) {
        QPixmap icon(project->options().icon);
        ui->lblICon->setPixmap(icon);
    }
}

void ProjectGeneralWidget::doSave()
{
    std::shared_ptr<Project> project = pMainWindow->project();
    if (!project)
        return;
    project->setName(ui->txtName->text().trimmed());

    project->options().encoding = ui->cbDefaultEncoding->currentText();

    int row = std::max(0,ui->lstType->currentRow());
    project->options().type = static_cast<ProjectType>(row);

    project->options().useGPP = ui->cbDefaultCpp->isChecked();
    project->options().supportXPThemes = ui->cbSupportXPTheme->isChecked();
    project->options().icon = mIconPath;
    project->saveOptions();
}