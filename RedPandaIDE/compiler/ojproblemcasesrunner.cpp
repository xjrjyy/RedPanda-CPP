/*
 * Copyright (C) 2020-2022 Roy Qu (royqh1979@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ojproblemcasesrunner.h"
#include "../utils.h"
#include "../settings.h"
#include "../systemconsts.h"
#include "../widgets/ojproblemsetmodel.h"
#include <QProcess>


OJProblemCasesRunner::OJProblemCasesRunner(const QString& filename, const QString& arguments, const QString& workDir,
                                           const QVector<POJProblemCase>& problemCases, QObject *parent):
    Runner(filename,arguments,workDir,parent)
{
    mProblemCases = problemCases;
    mBufferSize = 8192;
    mOutputRefreshTime = 1000;
    setWaitForFinishTime(100);
}

OJProblemCasesRunner::OJProblemCasesRunner(const QString& filename, const QString& arguments, const QString& workDir,
                                           POJProblemCase problemCase, QObject *parent):
    Runner(filename,arguments,workDir,parent)
{
    mProblemCases.append(problemCase);
    mBufferSize = 8192;
    mOutputRefreshTime = 1000;
    setWaitForFinishTime(100);
}

void OJProblemCasesRunner::runCase(int index,POJProblemCase problemCase)
{
    emit caseStarted(problemCase->getId(),index, mProblemCases.count());
    auto action = finally([this,&index, &problemCase]{
        emit caseFinished(problemCase->getId(), index, mProblemCases.count());
    });
    QProcess process;
    bool errorOccurred = false;

    process.setProgram(mFilename);
    process.setArguments(splitProcessCommand(mArguments));
    process.setWorkingDirectory(mWorkDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("PATH");
    QStringList pathAdded;
    if (pSettings->compilerSets().defaultSet()) {
        foreach(const QString& dir, pSettings->compilerSets().defaultSet()->binDirs()) {
            pathAdded.append(dir);
        }
    }
    pathAdded.append(pSettings->dirs().appDir());
    if (!path.isEmpty()) {
        path+= PATH_SEPARATOR + pathAdded.join(PATH_SEPARATOR);
    } else {
        path = pathAdded.join(PATH_SEPARATOR);
    }
    env.insert("PATH",path);
    process.setProcessEnvironment(env);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.connect(
                &process, &QProcess::errorOccurred,
                [&](){
        errorOccurred= true;
    });
    problemCase->output.clear();
    process.start();
    process.waitForStarted(5000);
    if (process.state()==QProcess::Running) {
        process.write(problemCase->input.toUtf8());
        process.closeWriteChannel();
    }
    QByteArray readed;
    QByteArray buffer;
    QByteArray output;
    int noOutputTime = 0;
    while (true) {
        process.waitForFinished(mWaitForFinishTime);
        readed = process.read(mBufferSize);
        buffer += readed;
        if (process.state()!=QProcess::Running) {
            break;
        }
        if (mStop) {            
            process.closeReadChannel(QProcess::StandardOutput);
            process.closeReadChannel(QProcess::StandardError);
            process.closeWriteChannel();
            process.terminate();
            process.kill();
            break;
        }
        if (errorOccurred)
            break;
        if (buffer.length()>=mBufferSize || noOutputTime > mOutputRefreshTime) {
            if (!buffer.isEmpty()) {
                emit newOutputGetted(problemCase->getId(),QString::fromLocal8Bit(buffer));
                output.append(buffer);
                buffer.clear();
            }
            noOutputTime = 0;
        } else {
            noOutputTime += mWaitForFinishTime;
        }
    }
    if (process.state() == QProcess::ProcessState::NotRunning)
        buffer += process.readAll();
    emit newOutputGetted(problemCase->getId(),QString::fromLocal8Bit(buffer));
    output.append(buffer);
    if (errorOccurred) {
        //qDebug()<<"process error:"<<process.error();
        switch (process.error()) {
        case QProcess::FailedToStart:
            emit runErrorOccurred(tr("The runner process '%1' failed to start.").arg(mFilename));
            break;
//        case QProcess::Crashed:
//            if (!mStop)
//                emit runErrorOccurred(tr("The runner process crashed after starting successfully."));
//            break;
        case QProcess::Timedout:
            emit runErrorOccurred(tr("The last waitFor...() function timed out."));
            break;
        case QProcess::WriteError:
            emit runErrorOccurred(tr("An error occurred when attempting to write to the runner process."));
            break;
        case QProcess::ReadError:
            emit runErrorOccurred(tr("An error occurred when attempting to read from the runner process."));
            break;
        default:
            break;
        }
    }
    problemCase->output = QString::fromLocal8Bit(output);
}

void OJProblemCasesRunner::run()
{
    emit started();
    auto action = finally([this]{
        emit terminated();
    });
    for (int i=0; i < mProblemCases.size(); i++) {
        if (mStop)
            break;
        POJProblemCase problemCase = mProblemCases[i];
        runCase(i,problemCase);
    }
}

int OJProblemCasesRunner::waitForFinishTime() const
{
    return mWaitForFinishTime;
}

void OJProblemCasesRunner::setWaitForFinishTime(int newWaitForFinishTime)
{
    mWaitForFinishTime = newWaitForFinishTime;
}

int OJProblemCasesRunner::outputRefreshTime() const
{
    return mOutputRefreshTime;
}

void OJProblemCasesRunner::setOutputRefreshTime(int newOutputRefreshTime)
{
    mOutputRefreshTime = newOutputRefreshTime;
}

int OJProblemCasesRunner::bufferSize() const
{
    return mBufferSize;
}

void OJProblemCasesRunner::setBufferSize(int newBufferSize)
{
    mBufferSize = newBufferSize;
}


