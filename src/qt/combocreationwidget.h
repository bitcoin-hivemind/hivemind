/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef COMBOCREATIONWIDGET_H
#define COMBOCREATIONWIDGET_H

#include "decisioncreationwidget.h"
#include "decisionmarketcreationwidget.h"
#include "json/json_spirit_writer_template.h"

#include <QWidget>
#include <QHBoxLayout>

namespace Ui {
class ComboCreationWidget;
}

class ComboCreationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ComboCreationWidget(QWidget *parent = 0);
    ~ComboCreationWidget();

private:
    Ui::ComboCreationWidget *ui;

    DecisionCreationWidget *decisionCreationWidget;
    DecisionMarketCreationWidget *marketCreationWidget;

    json_spirit::Array createComboArray();
    int updateIndex;

signals:
    /** Signal raised when json_spirit::Array for new Combo (Market & Decision) is created */
    void newComboArray(const json_spirit::Array &array);

    /** Signal raised for user input errors on combo creation widget */
    void inputError(const QString &error);

    /** Signal raised when user updates a decision array */
    void updatedComboArray(const json_spirit::Array array);

public slots:
    // Load a json_spirit array for editing
    void editArray(json_spirit::Array array);

private slots:
    void on_pushButtonCreateCombo_clicked();
    void on_pushButtonUpdateCombo_clicked();
};

#endif // COMBOCREATIONWIDGET_H
