/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "combocreationwidget.h"
#include "ui_combocreationwidget.h"

#include "decisioncreationwidget.h"

ComboCreationWidget::ComboCreationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ComboCreationWidget)
{
    ui->setupUi(this);

    decisionCreationWidget = new DecisionCreationWidget(this);
    decisionCreationWidget->comboCreationUI();

    marketCreationWidget = new DecisionMarketCreationWidget(this);
    marketCreationWidget->comboCreationUI();

    ui->frameDecision->layout()->addWidget(decisionCreationWidget);
    ui->frameMarket->layout()->addWidget(marketCreationWidget);

    updateIndex = -1;
    ui->pushButtonUpdateCombo->hide();
}

ComboCreationWidget::~ComboCreationWidget()
{
    delete ui;
}

json_spirit::Array ComboCreationWidget::createComboArray()
{
    json_spirit::Array decision = this->decisionCreationWidget->createDecisionArray();
    json_spirit::Array market = this->marketCreationWidget->createDecisionMarketArray();
    json_spirit::Array combo;

    // Add decision array to combo array
    for (unsigned int i = 0; i < decision.size(); i++) {
        combo.push_back(decision.at(i));
    }

    // Add market to combo array
    for (unsigned int i = 0; i < market.size(); i++) {
        combo.push_back(market.at(i));
    }

    // Type of array
    combo.push_back("combo");

    return combo;
}

void ComboCreationWidget::on_pushButtonCreateCombo_clicked()
{
    json_spirit::Array comboArray = createComboArray();
    if (comboArray.size() > 1) {
        emit newComboArray(comboArray);
    }
}

void ComboCreationWidget::editArray(json_spirit::Array array)
{
    if (array.size() < 19) {
        return;
    }

    // Check for update index from model
    updateIndex = array.back().get_int();
    array.pop_back();

    // Confirm type
    std::string type = json_spirit::write_string(array.back(), true);
    if (type != "\"combo\"") return;
    array.pop_back();

    // Split the combo into respective decision and market arrays
    json_spirit::Array decisionParams;
    json_spirit::Array marketParams;

    // Binary Decision & Market
    if (array.size() == 19) {
        // Perform checks
        std::string marketType = json_spirit::write_string(array.back(), true);
        if (marketType != "\"market\"") return;

        std::string decisionType = json_spirit::write_string(array.at(6), true);
        if (decisionType != "\"decision\"") return;

        for (unsigned int i = 0; i < array.size(); i++) {
            // Get decision params
            if (i < 7) {
                decisionParams.push_back(array.at(i));
            }
            // Get market params
            if (i >= 7 && i < 19) {
                marketParams.push_back(array.at(i));
            }
        }
    }

    // Scaled Decision & Market
    if (array.size() == 21) {
        // Perform checks
        std::string marketType = json_spirit::write_string(array.back(), true);
        if (marketType != "\"market\"") return;

        std::string decisionType = json_spirit::write_string(array.at(8), true);
        if (decisionType != "\"decision\"") return;

        for (unsigned int i = 0; i < array.size(); i++) {
            // Get decision params
            if (i < 9) {
                decisionParams.push_back(array.at(i));
            }
            // Get market params
            if (i >= 9 && i < 21) {
                marketParams.push_back(array.at(i));
            }
        }
    }

    if (updateIndex >= 0) {
        // Load the arrays in their respective widgets for editing
        decisionCreationWidget->editArray(decisionParams);
        marketCreationWidget->editArray(marketParams);
        ui->pushButtonCreateCombo->hide();
        ui->pushButtonUpdateCombo->show();
    }
}

void ComboCreationWidget::on_pushButtonUpdateCombo_clicked()
{
    json_spirit::Array updatedCombo = createComboArray();
    if (!updatedCombo.empty() && updateIndex >= 0) {
        updatedCombo.push_back(updateIndex);
        emit updatedComboArray(updatedCombo);
    }
}
