/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef AUTHORPENDINGTABLEMODEL_H
#define AUTHORPENDINGTABLEMODEL_H

#include "combocreationwidget.h"
#include "decisioncreationwidget.h"
#include "decisionmarketcreationwidget.h"
#include "json/json_spirit_writer_template.h"
#include "txdb.h"
#include "txmempool.h"

#include <QAbstractTableModel>
#include <QList>

extern CTxMemPool mempool;
extern CMarketTreeDB *pmarkettree;

class AuthorPendingTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AuthorPendingTableModel(QObject *parent = 0);
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    void finalize();
    void finalizeCombo(json_spirit::Array params, unsigned int index);

signals:
    void finalizeError(const QString &errorMessage);
    void finalizeComplete();

public slots:
    void on_tableView_doubleClicked(const QModelIndex &index);

    // Edit on double click
    void editCombo(const QModelIndex &index);
    void editDecision(const QModelIndex &index);
    void editDecisionMarket(const QModelIndex &index);

    // Receive new arrays
    void receivePendingCombo(json_spirit::Array array);
    void receivePendingDecision(json_spirit::Array array);
    void receivePendingDecisionMarket(json_spirit::Array array);

    // Receive array updates
    void receiveUpdatedCombo(json_spirit::Array array);
    void receiveUpdatedDecision(json_spirit::Array array);
    void receiveUpdatedDecisionMarket(json_spirit::Array array);

private:
    QList<json_spirit::Array> pending;

    ComboCreationWidget *comboCreationWidget;
    DecisionCreationWidget *decisionCreationWidget;
    DecisionMarketCreationWidget *decisionMarketCreationWidget;

};

#endif // AUTHORPENDINGTABLEMODEL_H
