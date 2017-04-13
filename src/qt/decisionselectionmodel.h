/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef DECISIONSELECTIONMODEL_H
#define DECISIONSELECTIONMODEL_H

#include "primitives/market.h"

#include <QAbstractTableModel>
#include <QList>
#include <QObject>

class DecisionSelectionModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DecisionSelectionModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;

signals:

public slots:
    void on_tableView_doubleClicked(const QModelIndex &index);
    void loadDecisions(QList<marketDecision *> decisions);

private:
    QList<marketDecision *> model;

};

#endif // DECISIONSELECTIONMODEL_H
