/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "updatee2eesharemetadatajob.h"
#include "clientsideencryption.h"
#include "clientsideencryptionjobs.h"
#include "folderman.h"

namespace OCC
{
Q_LOGGING_CATEGORY(lcCreateE2eeShareJob, "nextcloud.gui.createe2eesharejob", QtInfoMsg)

UpdateE2eeShareMetadataJob::UpdateE2eeShareMetadataJob(const QString &sharePath,
                                       const ShareePtr &sharee,
                                       const Share::Permissions desiredPermissions,
                                       const QSharedPointer<ShareManager> &shareManager,
                                       const AccountPtr &account,
                                       const QByteArray &folderId,
                                       const QString &folderAlias,
                                       const QString &password,
                                       QObject *parent)
    : QObject{parent}
    , _sharePath(sharePath)
    , _sharee(sharee)
    , _desiredPermissions(desiredPermissions)
    , _manager(shareManager)
    , _account(account)
    , _folderId(folderId)
    , _folderAlias(folderAlias)
    , _password(password)
{
    connect(this, &UpdateE2eeShareMetadataJob::certificateReady, this, &UpdateE2eeShareMetadataJob::slotCertificateReady);
    connect(this, &UpdateE2eeShareMetadataJob::finished, this, &UpdateE2eeShareMetadataJob::deleteLater);
}

QString UpdateE2eeShareMetadataJob::password() const
{
    return _password;
}

QString UpdateE2eeShareMetadataJob::sharePath() const
{
    return _sharePath;
}

Share::Permissions UpdateE2eeShareMetadataJob::desiredPermissions() const
{
    return _desiredPermissions;
}

ShareePtr UpdateE2eeShareMetadataJob::sharee() const
{
    return _sharee;
}

void UpdateE2eeShareMetadataJob::start()
{
    _account->e2e()->fetchFromKeyChain(_account, _sharee->shareWith());
    connect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain);
}

void UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain(QSslCertificate certificate)
{
    disconnect(_account->e2e(), &ClientSideEncryption::certificateFetchedFromKeychain, this, &UpdateE2eeShareMetadataJob::slotCertificateFetchedFromKeychain);
    if (!certificate.isValid()) {
        // get sharee's public key
        _account->e2e()->getUsersPublicKeyFromServer(_account, {_sharee->shareWith()});
        connect(_account->e2e(), &ClientSideEncryption::certificatesFetchedFromServer, this, &UpdateE2eeShareMetadataJob::slotCertificatesFetchedFromServer);
        return;
    }
    emit certificateReady(certificate);
}

void UpdateE2eeShareMetadataJob::slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results)
{
    const auto certificate = results.isEmpty() ? QSslCertificate{} : results.value(_sharee->shareWith());
    if (!certificate.isValid()) {
        emit certificateReady(certificate);
        return;
    }
    _account->e2e()->writeCertificate(_account, _sharee->shareWith(), certificate);
    connect(_account->e2e(), &ClientSideEncryption::certificateWriteComplete, this, &UpdateE2eeShareMetadataJob::certificateReady);
}

void UpdateE2eeShareMetadataJob::slotCertificateReady(QSslCertificate certificate)
{
    _shareeCertificate = certificate;

    if (!certificate.isValid()) {
        emit finished(404, tr("Could not fetch publicKey for user %1").arg(_sharee->shareWith()));
    } else {
        slotFetchFolderMetadata();
    }
}

void UpdateE2eeShareMetadataJob::slotFetchFolderMetadata()
{
    const auto job = new GetMetadataApiJob(_account, _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &UpdateE2eeShareMetadataJob::slotMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &UpdateE2eeShareMetadataJob::slotMetadataError);
    job->start();
}

void UpdateE2eeShareMetadataJob::slotMetadataReceived(const QJsonDocument &json, int statusCode)
{
    qCDebug(lcCreateE2eeShareJob) << "Metadata received, applying it to the result list";

    _folderMetadata.reset(new FolderMetadata(_account, json.toJson(QJsonDocument::Compact), statusCode));

    slotLockFolder();
}

void UpdateE2eeShareMetadataJob::slotMetadataError(const QByteArray &folderId, int httpReturnCode)
{
    qCWarning(lcCreateE2eeShareJob) << "E2EE Metadata job error. Trying to proceed without it." << folderId << httpReturnCode;
    emit finished(404, tr("Could not fetch metadata for folder %1").arg(QString::fromUtf8(_folderId)));
}
void UpdateE2eeShareMetadataJob::slotLockFolder()
{
    const auto folder = FolderMan::instance()->folder(_folderAlias);
    if (!folder && !folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }
    const auto lockJob = new LockEncryptFolderApiJob(_account, _folderId, folder->journalDb(), _account->e2e()->_publicKey, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &UpdateE2eeShareMetadataJob::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &UpdateE2eeShareMetadataJob::slotFolderLockedError);
    lockJob->start();
}

void UpdateE2eeShareMetadataJob::slotUnlockFolder()
{
    ASSERT(!_isUnlockRunning);

    if (_isUnlockRunning) {
        qWarning() << "Double-call to unlockFolder.";
        return;
    }

    _isUnlockRunning = true;

    const auto folder = FolderMan::instance()->folder(_folderAlias);
    if (!folder && !folder->journalDb()) {
        emit finished(404, tr("Could not find local folder for %1").arg(QString::fromUtf8(_folderId)));
        return;
    }

    qDebug() << "Calling Unlock";
    const auto unlockJob = new UnlockEncryptFolderApiJob(_account, _folderId, _folderToken, folder->journalDb(), this);

    connect(unlockJob, &UnlockEncryptFolderApiJob::success, [this](const QByteArray &folderId) {
        qDebug() << "Successfully Unlocked";
        _folderToken = "";
        _folderId = "";
        _isFolderLocked = false;

        slotFolderUnlocked(folderId, 200);
        _isUnlockRunning = false;
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, [this](const QByteArray &folderId, int httpStatus) {
        qDebug() << "Unlock Error";

        slotFolderUnlocked(folderId, httpStatus);
        _isUnlockRunning = false;
    });
    unlockJob->start();
}

void UpdateE2eeShareMetadataJob::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(lcCreateE2eeShareJob) << "Folder" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    // Should I use a mutex here?
    _currentLockingInProgress = true;
    _folderToken = token;
    _isFolderLocked = true;

    if (!_folderMetadata->addShareRecipient(_sharee->shareWith(), _shareeCertificate)) {
        emit finished(403, tr("Could not add a sharee recipient %1, for folder %2").arg(_sharee->shareWith()).arg(QString::fromUtf8(_folderId)));
        return;
    }

    const auto job = new UpdateMetadataApiJob(_account, _folderId, _folderMetadata->encryptedMetadata(), _folderToken);

    connect(job, &UpdateMetadataApiJob::success, this, &UpdateE2eeShareMetadataJob::slotUpdateMetadataSuccess);
    connect(job, &UpdateMetadataApiJob::error, this, &UpdateE2eeShareMetadataJob::slotUpdateMetadataError);
    job->start();
}

void UpdateE2eeShareMetadataJob::slotUpdateMetadataSuccess(const QByteArray &folderId)
{
    Q_UNUSED(folderId);
    qCDebug(lcCreateE2eeShareJob) << "Uploading of the metadata success, Encrypting the file";
    qCDebug(lcCreateE2eeShareJob) << "Finalizing the upload part, now the actuall uploader will take over";
    slotUnlockFolder();
}

void UpdateE2eeShareMetadataJob::slotUpdateMetadataError(const QByteArray &folderId, int httpErrorResponse)
{
    qCDebug(lcCreateE2eeShareJob) << "Update metadata error for folder" << folderId << "with error" << httpErrorResponse;
    qCDebug(lcCreateE2eeShareJob()) << "Unlocking the folder.";
    slotUnlockFolder();
}

void UpdateE2eeShareMetadataJob::slotFolderUnlocked(const QByteArray &folderId, int httpStatus)
{
    const QString message = httpStatus != 200 ? tr("Failed to unlock a folder.") : QString{};
    emit finished(httpStatus, message);
}

void UpdateE2eeShareMetadataJob::slotFolderLockedError(const QByteArray &folderId, int httpErrorCode)
{
    Q_UNUSED(httpErrorCode);
    qCDebug(lcCreateE2eeShareJob) << "Folder" << folderId << "Coundn't be locked.";
    emit finished(404, tr("Could not lock a folder %1").arg(QString::fromUtf8(_folderId)));
}

}