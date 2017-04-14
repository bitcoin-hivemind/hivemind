// Copyright (c) 2015 The Hivemind Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern "C" {
#include "linalg/src/tc_mat.h"
}
#include "resolvevotedialog.h"
#include "resolvevoteinputtablemodel.h"

#include <QMessageBox>

ResolveVoteInputTableModel::ResolveVoteInputTableModel()
    : QAbstractTableModel(0),
    resolveVoteDialog(0),
    voteptr(0)
{

}

ResolveVoteInputTableModel::~ResolveVoteInputTableModel()
{

}

int ResolveVoteInputTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    // 3 (min number of rows) + number of rows initialized or 0 if none
    return 3 + ((voteptr && *voteptr)? (*voteptr)->nr: 0);
}

int ResolveVoteInputTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    // 1 (min number of columns) + number of columns initialized or 0 if none
    return 1 + ((voteptr && *voteptr)? (*voteptr)->nc: 0);
}

QVariant ResolveVoteInputTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    if (role == Qt::DisplayRole)
    {
        if (!voteptr || !*voteptr || !(*voteptr))
            return QVariant();

        uint32_t row = index.row();
        uint32_t col = index.column();

        if ((row < 3+(*voteptr)->nr) && (col < 1+(*voteptr)->nc))
        {
            double value = 0.0;            

            if (col == 0) { /* Starting Reputations */
                if (row >= 3)
                    value = (*voteptr)->rvecs[TC_VOTE_OLD_REP]->a[row-3][0];
                else
                    return QVariant();
            }
            else
            if (row == 0) /* Binary/Scalar */
                value = (*voteptr)->cvecs[TC_VOTE_IS_BINARY]->a[0][col-1];
            else
            if (row == 1) /* Minimum */
                value = 0.0;
            else
            if (row == 2) /* Maximum */
                value = 1.0;
            else
                value = (*voteptr)->M->a[row-3][col-1];

            if (value != (*voteptr)->NA) {
                return QVariant(QString::number(value, 'f', 8));
            }

            return QVariant(QString("NA"));
        }
    }
    else
    if (role == Qt::TextAlignmentRole)
        return (int)(Qt::AlignRight|Qt::AlignVCenter);

    return QVariant();
}

QVariant ResolveVoteInputTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            // Oldrep and Decision headers for input view
            char tmp[32];
            if (section == 0)
                snprintf(tmp, sizeof(tmp), "Old Rep");
            else
                snprintf(tmp, sizeof(tmp), "Decision %u", section);
            return QVariant(QString(tmp));
        }
        else
        if (role == Qt::TextAlignmentRole)
            return (int)(Qt::AlignLeft|Qt::AlignVCenter);
        else
        if (role == Qt::ToolTipRole)
        {
        }
    }

    if(orientation == Qt::Vertical)
    {
        if (role == Qt::DisplayRole) {
            if (section == 0) return QVariant(tr("Binary/Scalar"));
            if (section == 1) return QVariant(tr("Minumum"));
            if (section == 2) return QVariant(tr("Maximum"));
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "Voter %u", section - 2);
            return QVariant(QString(tmp));
        }
        else
        if (role == Qt::TextAlignmentRole)
            return (int)(Qt::AlignLeft|Qt::AlignVCenter);
        else
        if (role == Qt::ToolTipRole)
        {
        }
    }

    return QVariant();
}

Qt::ItemFlags ResolveVoteInputTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void ResolveVoteInputTableModel::onDataChange()
{
    uint32_t nr = 3 + ((voteptr && *voteptr)? (*voteptr)->nr: 0);
    uint32_t nc = (voteptr && *voteptr)? (*voteptr)->nc: 0;
    if (nc) {
        QModelIndex topLeft = index(0, 0, QModelIndex());
        QModelIndex bottomRight = index(nr-1, nc-1, QModelIndex());
        emit dataChanged(topLeft, bottomRight);
    }
}

bool ResolveVoteInputTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    if (!index.isValid())
        return false;

    if (value.isNull())
        return false;

    /* Accepted input types for QVariant "value" */
    bool isNA = false;
    double dvalue = 0.0;

    if (value.toString() == "NA") {
        isNA = true;
    } else {
        bool ok;
        dvalue = value.toDouble(&ok);
        if (!ok) {
            QMessageBox msgBox;
            msgBox.setText(tr("Enter a valid number or NA"));
            msgBox.exec();
            return false;
        }
    }

    int row = index.row();
    int col = index.column();
    if ((row < 3+(int)(*voteptr)->nr)
        && (col < 1 + (int)(*voteptr)->nc))
    {
        if (col == 0) {
           if (row >= 3)
              (*voteptr)->rvecs[TC_VOTE_OLD_REP]->a[row-3][0] = dvalue;
        }
        else
        if (row == 0)
           (*voteptr)->cvecs[TC_VOTE_IS_BINARY]->a[0][col-1] = (dvalue > 0.5)? 1.0: 0.0;
        else
        if (row == 1)
            ;
        else
        if (row == 2)
            ;
        else
           (*voteptr)->M->a[row-3][col-1] = (isNA)? (*voteptr)->NA:  dvalue;

        emit dataChanged(index, index);

        if (resolveVoteDialog)
            resolveVoteDialog->onInputChange();
    }
    return false;
}
