/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef AUTHORVIEW_H
#define AUTHORVIEW_H

#include "authorpendingtablemodel.h"
#include "combocreationwidget.h"
#include "decisioncreationwidget.h"
#include "decisionmarketcreationwidget.h"

#include <QList>
#include <QTableView>
#include <QWidget>

namespace Ui {
class AuthorView;
}

class AuthorView : public QWidget
{
    Q_OBJECT

public:
    explicit AuthorView(QWidget *parent = 0);
    ~AuthorView();

private slots:
    void on_pushButtonCreateCombo_clicked();
    void on_pushButtonCreateDecision_clicked();
    void on_pushButtonCreateMarket_clicked();
    void on_pushButtonFinalize_clicked();
    void on_finalizeError(const QString &errorMessage);
    void calculateFees();

signals:
    void newPendingCombo(const json_spirit::Array array);
    void newPendingDecision(const json_spirit::Array array);
    void newPendingDecisionMarket(const json_spirit::Array array);

private:
    Ui::AuthorView *ui;

    ComboCreationWidget *comboCreationWidget;
    DecisionCreationWidget *decisionCreationWidget;
    DecisionMarketCreationWidget *decisionMarketCreationWidget;

    AuthorPendingTableModel *pendingTableModel;
    QTableView *pendingTableView;

};

#endif // AUTHORVIEW_H
