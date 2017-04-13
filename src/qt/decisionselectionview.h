/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef DECISIONSELECTIONVIEW_H
#define DECISIONSELECTIONVIEW_H

#include "decisionselectionmodel.h"

#include <QTableView>
#include <QWidget>
#include <QStringList>

namespace Ui {
class DecisionSelectionView;
}

class DecisionSelectionView : public QWidget
{
    Q_OBJECT

public:
    explicit DecisionSelectionView(QWidget *parent = 0);
    ~DecisionSelectionView();

public slots:
    void loadDecisions(QList<marketDecision *> decisions);
    void on_table_doubleClicked(QModelIndex index);

signals:
    void decisionSelected(QString decisionHex);
    void multipleDecisionsSelected(QStringList hexList);
    void done();

private slots:
    void on_pushButtonDone_clicked();

private:
    Ui::DecisionSelectionView *ui;

    DecisionSelectionModel *decisionSelectionModel;
    QTableView *decisionSelectionTable;
};

#endif // DECISIONSELECTIONVIEW_H
