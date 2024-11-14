/***************************************************************************
 *   Copyright © 2012 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "workerdaemon.h"

// Qt includes
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QProcess>

// Apt-pkg includes
#include <apt-pkg/configuration.h>

// Own includes
#include "aptworker.h"
#include "qaptauthorization.h"
#include "transaction.h"
#include "transactionqueue.h"
#include "workeradaptor.h"
#include "urihelper.h"

#define IDLE_TIMEOUT 60*60*1000 // 30 seconds

WorkerDaemon::WorkerDaemon(int &argc, char **argv)
    : QCoreApplication(argc, argv)
    , m_queue(nullptr)
    , m_worker(nullptr)
    , m_workerThread(nullptr)
{
    m_worker = new AptWorker(nullptr);
    m_queue = new TransactionQueue(this, m_worker);

    m_TransTimer = 0;
    m_Deepin_deb_installer_PID = 0;
    m_IDLE_TIMEOUT = getIdleTimeout()*1000;

    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);
    m_workerThread->start();
    connect(m_workerThread, SIGNAL(finished()), this, SLOT(quit()));

    // Invoke with Qt::QueuedConnection since the Qt event loop isn't up yet
    QMetaObject::invokeMethod(m_worker, "init", Qt::QueuedConnection);
    connect(m_queue, SIGNAL(queueChanged(QString,QStringList)),
            this, SIGNAL(transactionQueueChanged(QString,QStringList)),
            Qt::QueuedConnection);
    qRegisterMetaType<Transaction *>("Transaction *");
    QApt::DownloadProgress::registerMetaTypes();

    // Start up D-Bus service
    new WorkerAdaptor(this);

    if (!QDBusConnection::systemBus().registerService(QLatin1String(s_workerReverseDomainName))) {
        // Another worker is already here, quit
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        qWarning() << "Couldn't register service" << QDBusConnection::systemBus().lastError().message();
        return;
    }

    if (!QDBusConnection::systemBus().registerObject(QLatin1String("/"), this)) {
        QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
        qWarning() << "Couldn't register object";
        return;
    }

    // Quit if we've not run a job for a while
    m_idleTimer = new QTimer(this);
    m_idleTimer->start(IDLE_TIMEOUT);
    connect(m_idleTimer, SIGNAL(timeout()), this, SLOT(checkIdle()), Qt::QueuedConnection);
}

void WorkerDaemon::checkIdle()
{
    quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (!m_worker->currentTransaction() &&
        currentTime - m_worker->lastActiveTimestamp() > IDLE_TIMEOUT &&
        m_queue->isEmpty()) {
        m_worker->quit();
    }
}

int WorkerDaemon::dbusSenderUid() const
{
    return connection().interface()->serviceUid(message().service()).value();
}

void WorkerDaemon::setTransTimer(quint64 timer)
{
    m_TransTimer = timer;
}

int WorkerDaemon::getIdleTimeout()
{
    QString confpath("/etc/libqapt3/libqapt3.conf");
    //the timeout default value is 30 seconds.
    int result=30;
    if (QFile::exists(confpath))
    {
        QSettings confinfo(confpath,QSettings::IniFormat);
        QString tmp_TimeOut = confinfo.value("/basic_auth_config/expand_time").toString();
        if (!tmp_TimeOut.isEmpty())
            result = tmp_TimeOut.toInt()*60;
    }
    return result;
}

bool WorkerDaemon::judgeAuthen()
{
    //Determine whether authentication is required
    //get PID of deepin_deb_installer 
    QProcess process;
    QStringList options;
    QString tmp_PID("");

    options<<"-c" <<"ps aux --sort -uid |grep deepin-deb-installer |head -n 1 |awk '{print $2}'";
    process.start("/bin/bash", options);
    process.waitForFinished();
    tmp_PID = process.readAllStandardOutput();
    tmp_PID.remove(QChar('\n'), Qt::CaseInsensitive);
    
    //judge if need to get the user authentication.
    if (m_Deepin_deb_installer_PID != tmp_PID.toInt())
    {
        //if tmp_PID != m_Deepin_deb_installer_PID , means the deepin_deb_installer is reopen and need
        //to get the user authentication.
        m_Deepin_deb_installer_PID = tmp_PID.toInt();
        m_TransTimer = 0;
        return true;
    }
    else
    {
        //if pid is the same. Then need to judge the difference between the last authentication time 
        //and current time, if it is greater than the set value, means need to get user authentication.
        quint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - m_TransTimer > m_IDLE_TIMEOUT)
        {
            return true;
        }
    }
    return false;
}

Transaction *WorkerDaemon::createTransaction(QApt::TransactionRole role, QVariantMap instructionsList)
{
    int uid = dbusSenderUid();

    // Create a transaction. It will add itself to the queue
    Transaction *trans = new Transaction(m_queue, uid, role, instructionsList, this);
    trans->setService(message().service());

    return trans;
}

QString WorkerDaemon::updateCache()
{
    Transaction *trans = createTransaction(QApt::UpdateCacheRole);

    return trans->transactionId();
}

QString WorkerDaemon::installFile(const QString &file)
{
    Transaction *trans = createTransaction(QApt::InstallFileRole);
    trans->setFilePath(file);

    return trans->transactionId();
}

QString WorkerDaemon::commitChanges(QVariantMap instructionsList)
{
    Transaction *trans = createTransaction(QApt::CommitChangesRole,
                                           instructionsList);

    return trans->transactionId();
}

QString WorkerDaemon::upgradeSystem(bool safeUpgrade)
{
    Transaction *trans = createTransaction(QApt::UpgradeSystemRole);
    trans->setSafeUpgrade(safeUpgrade);

    return trans->transactionId();
}

QString WorkerDaemon::downloadArchives(const QStringList &packageNames, const QString &dest)
{
    QVariantMap packages;

    for (const QString &pkg : packageNames) {
        packages.insert(pkg, 0);
    }

    Transaction *trans = createTransaction(QApt::DownloadArchivesRole, packages);
    trans->setFilePath(dest);

    return trans->transactionId();
}

bool WorkerDaemon::writeFileToDisk(const QString &contents, const QString &path)
{
    if (!QApt::Auth::authorize(dbusActionUri("writefiletodisk"), message().service())) {
        qDebug() << "Failed to authorize!!";
        return false;
    }

	QString file_path;
	QFileInfo f;
	f = QFileInfo(path);
	file_path = f.absolutePath();
    QList<QString> whitelist;
    quint8 allow_write_flag = 0;
    whitelist<<"/etc/apt/";
    for( int i=0; i< whitelist.size(); ++i){
        if(file_path.startsWith (whitelist[i], Qt::CaseSensitive)){
            allow_write_flag = 1;
        } 
    }
    if(allow_write_flag == 0){
        qDebug()<< "Refuse to write";
        return false ;
    }

    QFile file(path);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(contents.toLatin1());
        return true;
    }

    qDebug() << "Failed to write file to disk: " << file.errorString();
    return false;
}

bool WorkerDaemon::copyArchiveToCache(const QString &archivePath)
{
    if (!QApt::Auth::authorize(dbusActionUri("writefiletodisk"), message().service())) {
        return false;
    }

    QString cachePath = QString::fromStdString(_config->FindDir("Dir::Cache::Archives"));
    // Filename
    cachePath += archivePath.right(archivePath.size() - archivePath.lastIndexOf('/'));

    if (QFile::exists(cachePath)) {
        // Already copied
        return true;
    }

    return QFile::copy(archivePath, cachePath);
}
