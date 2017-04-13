/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "decisionselectionmodel.h"

DecisionSelectionModel::DecisionSelectionModel(QObject *parent) :
    QAbstractTableModel(parent)
{
}

int DecisionSelectionModel::rowCount(const QModelIndex &parent) const
{
    return model.size();
}

int DecisionSelectionModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant DecisionSelectionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    switch(role) {
    case Qt::DisplayRole:
    {
        // Decision prompt
        if (col == 0) {
            QString decisionPrompt = QString::fromStdString(model.at(row)->prompt);

            return decisionPrompt;
        }
        // Decision ID
        if (col == 1) {
            QString decisionID = QString::fromStdString(model.at(row)->GetHash().GetHex());

            return decisionID;
        }
        break;
    }
    }

    return QVariant();

}

QVariant DecisionSelectionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal) {
            switch (section)
            {
            case 0:
                return QString("Prompt");
                break;
            case 1:
                return QString("ID");
            }
        }
    }
    return QVariant();
}

void DecisionSelectionModel::on_tableView_doubleClicked(const QModelIndex &index)
{

}

void DecisionSelectionModel::loadDecisions(QList<marketDecision *> decisions)
{
    if (decisions.isEmpty()) return;

    beginInsertRows(QModelIndex(), 0, decisions.size() - 1);

    for (int i = 0; i < decisions.size(); i++) {
        model.push_back(decisions.at(i));
    }

    endInsertRows();
}
