/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#pragma once

#include <QDateTime>
#include <QObject>

namespace OCC
{
namespace Quick
{

class DateFieldBackend : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QDateTime dateTime READ dateTime WRITE setDateTime NOTIFY dateTimeChanged)
    Q_PROPERTY(qint64 dateTimeMsecs READ dateTimeMsecs WRITE setDateTimeMsecs NOTIFY dateTimeChanged)
    Q_PROPERTY(QString dateTimeString READ dateTimeString WRITE setDateTimeString NOTIFY dateTimeChanged)

    Q_PROPERTY(QDateTime minimumDateTime READ minimumDateTime WRITE setMinimumDateTime NOTIFY minimumDateTimeChanged)
    Q_PROPERTY(qint64 minimumDateTimeMsecs READ minimumDateTimeMsecs WRITE setMinimumDateTimeMsecs NOTIFY minimumDateTimeChanged)
public:
    explicit DateFieldBackend() = default;

    [[nodiscard]] QDateTime dateTime() const;
    [[nodiscard]] qint64 dateTimeMsecs() const;
    [[nodiscard]] QString dateTimeString() const;

    [[nodiscard]] QDateTime minimumDateTime() const;
    [[nodiscard]] qint64 minimumDateTimeMsecs() const;

public slots:
    void setDateTime(const QDateTime &dateTime);
    void setDateTimeMsecs(const qint64 dateTimeMsecs);
    void setDateTimeString(const QString &dateTimeString);

    void setMinimumDateTime(const QDateTime &minimumDateTime);
    void setMinimumDateTimeMsecs(const qint64 minimumDateTimeMsecs);

signals:
    void dateTimeChanged();
    void minimumDateTimeChanged();

private:
    QDateTime _dateTime;
    QDateTime _minimumDateTime;
};

} // namespace Quick
} // namespace OCC