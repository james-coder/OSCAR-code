/* Profile Selector Header
 *
 * Copyright (c) 2019-2025 The OSCAR Team
 * Copyright (c) 2018 Mark Watkins 
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the source code
 * for more details. */

#ifndef PROFILESELECTOR_H
#define PROFILESELECTOR_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "SleepLib/profiles.h"

namespace Ui {
class ProfileSelector;
}

class MySortFilterProxyModel2:public QSortFilterProxyModel
{
    Q_OBJECT
  public:
    MySortFilterProxyModel2(QObject *parent = 0);

    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;

    void setDateColumn(int column) {
        dateColumn = column;
    }

    void setDateFormat(const QString &format) {
        dateFormat = format;
    }

  protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        // Check if this is the date column
        if (left.column() == dateColumn) {
            QString leftString = sourceModel()->data(left).toString();
            QString rightString = sourceModel()->data(right).toString();

            // Parse strings back to QDate
            QDate leftDate = QDate::fromString(leftString, dateFormat);
            QDate rightDate = QDate::fromString(rightString, dateFormat);

            // If both are valid dates, compare them
            if (leftDate.isValid() && rightDate.isValid()) {
                return leftDate < rightDate;
            }
        }

        // For all other columns, use default case-insensitive comparison
        return QSortFilterProxyModel::lessThan(left, right);
  }

  private:
    int dateColumn = 4;
    QString dateFormat = QLocale::system().dateFormat(QLocale::ShortFormat);
};

class ProfileSelector : public QWidget
{
    Q_OBJECT

public:
    explicit ProfileSelector(QWidget *parent = 0);
    ~ProfileSelector();

    void updateProfileList();
    Profile *SelectProfile(QString profname, bool skippassword);
    void updateProfileHighlight(QString name);

private slots:
    void on_profileView_doubleClicked(const QModelIndex &index);

    void on_profileFilter_textChanged(const QString &arg1);

    void on_buttonOpenProfile_clicked();

    void on_buttonEditProfile_clicked();

    void on_buttonNewProfile_clicked();

    void on_buttonDestroyProfile_clicked();

    void on_diskSpaceInfo_linkActivated(const QString &link);

    void on_selectionChanged(const QModelIndex &current, const QModelIndex &previous);

    void on_resetFilterButton_clicked();

private:
    QString getProfileDiskInfo(Profile *profile);
    QString formatSize(qint64 size);

    Ui::ProfileSelector *ui;
    QStandardItemModel *model;
    MySortFilterProxyModel2 *proxy;

    bool showDiskUsage;
};

#endif // PROFILESELECTOR_H
