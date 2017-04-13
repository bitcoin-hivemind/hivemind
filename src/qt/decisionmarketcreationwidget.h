/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef DECISIONMARKETCREATIONWIDGET_H
#define DECISIONMARKETCREATIONWIDGET_H

#include "json/json_spirit_writer_template.h"
#include "decisionselectionview.h"

#include <QWidget>
#include <QStringList>

namespace Ui {
class DecisionMarketCreationWidget;
}

class DecisionMarketCreationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DecisionMarketCreationWidget(QWidget *parent = 0);
    ~DecisionMarketCreationWidget();

    json_spirit::Array createDecisionMarketArray();

private slots:
    void on_pushButtonCreateMarket_clicked();

    void on_pushButtonSelectDecision_clicked();

    void on_inputError(const QString &errorMessage);

    void on_pushButtonUpdateMarket_clicked();

    void on_toolButtonAdvanced_clicked();

public slots:
    // Load json_spirit array for editing
    void editArray(json_spirit::Array array);

    // Setup the UI for the combo creation widget
    void comboCreationUI();

    void decisionSelected(QString decisionHex);

    void multipleDecisionsSelected(QStringList hexList);

signals:
    /** Signal raised when json_spirit::Array for new Decision Market is created */
    void newDecisionMarketArray(const json_spirit::Array array);

    /** Signal raised when user updates a decision array */
    void updatedDecisionMarketArray(const json_spirit::Array array);

    /** Signal raised for user input errors on market creation widget */
    void inputError(const QString &error);

private:
    Ui::DecisionMarketCreationWidget *ui;
    int updateIndex;
    DecisionSelectionView *decisionSelection;
};

#endif // DECISIONMARKETCREATIONWIDGET_H
