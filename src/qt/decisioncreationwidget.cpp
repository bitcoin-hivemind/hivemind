/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "decisioncreationwidget.h"
#include "ui_decisioncreationwidget.h"

#include <QMessageBox>

DecisionCreationWidget::DecisionCreationWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DecisionCreationWidget)
{
    ui->setupUi(this);

    ui->pushButtonUpdateDecision->hide();
    updateIndex = -1;

    connect(this, SIGNAL(inputError(QString)),
            this, SLOT(on_inputError(QString)));
}

DecisionCreationWidget::~DecisionCreationWidget()
{
    delete ui;
}

json_spirit::Array DecisionCreationWidget::createDecisionArray()
{
    // Main branch ID
    QString branchID;
    if (ui->comboBoxBranch->currentText() == "Main") {
        branchID = "0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d";
    }

    // Grab user input from ui
    QString address = ui->lineEditOwnerAddr->text();
    QString prompt = ui->plainTextEditPrompt->toPlainText();
    int eventOverBy = ui->spinBoxEventOverBy->value();
    bool answerOptionality = false;
    if (ui->checkBoxVoteMandatory->isChecked()) {
        answerOptionality = false;
    } else {
        answerOptionality = true;
    }
    bool scaled = ui->radioButtonScaled->isChecked();
    double scaledMin = ui->doubleSpinBoxScaledMin->value();
    double scaledMax = ui->doubleSpinBoxScaledMax->value();

    bool error = false;

    // Perform basic checks and avoid wasting json calls
    if (branchID.isEmpty()) {
        emit inputError("You must select a branch!");
        error = true;
    }

    if (address.isEmpty()) {
        emit inputError("You must enter a valid Hivemind address!");
        error = true;
    }

    if (prompt.isEmpty()) {
        emit inputError("You must enter a valid prompt!");
        error = true;
    }

    if (eventOverBy == 0) {
        emit inputError("You must enter a valid ending date!");
        error = true;
    }

    if (scaled && (scaledMax == scaledMin)) {
        emit inputError("You must enter  valid scaled values!");
        error = true;
    }

    // Setup json_spirit array
    json_spirit::Array params;

    // Return empty array if there was an input error
    if (error) return params;

    // Add parameters to array
    params.push_back(address.toStdString());
    params.push_back(branchID.toStdString());
    params.push_back(prompt.toStdString());
    params.push_back(eventOverBy);
    params.push_back(answerOptionality);
    params.push_back(scaled);
    if (scaled) {
        params.push_back(scaledMin);
        params.push_back(scaledMax);
    }
    // Type of array
    params.push_back("decision");

    return params;
}

void DecisionCreationWidget::on_pushButtonCreateDecision_clicked()
{
    json_spirit::Array decisionArray = createDecisionArray();
    if (!decisionArray.empty()) {
        emit newDecisionArray(decisionArray);
    }
}

void DecisionCreationWidget::on_radioButtonBinary_clicked(bool checked)
{
    // Enable scale doublespinbox widgets for binary decision
    if (checked) {
        ui->doubleSpinBoxScaledMin->setEnabled(false);
        ui->doubleSpinBoxScaledMax->setEnabled(false);
    } else {
        ui->doubleSpinBoxScaledMin->setEnabled(true);
        ui->doubleSpinBoxScaledMax->setEnabled(true);
    }
}

void DecisionCreationWidget::on_radioButtonScaled_clicked(bool checked)
{
    // Enable scale doublespinbox widgets for scaled decision
    if (checked) {
        ui->doubleSpinBoxScaledMin->setEnabled(true);
        ui->doubleSpinBoxScaledMax->setEnabled(true);
    } else {
        ui->doubleSpinBoxScaledMin->setEnabled(false);
        ui->doubleSpinBoxScaledMax->setEnabled(false);
    }
}

void DecisionCreationWidget::on_inputError(const QString &errorMessage)
{
    QMessageBox errorMessageBox;
    errorMessageBox.setWindowTitle("Input error!");
    errorMessageBox.setText(errorMessage);
    errorMessageBox.setStandardButtons(QMessageBox::Ok);
    errorMessageBox.setDefaultButton(QMessageBox::Ok);
    errorMessageBox.exec();
}

void DecisionCreationWidget::editArray(json_spirit::Array array)
{
    if (array.size() < 6) {
        return;
    }

    // Load owner address
    json_spirit::Value ownerAddr = array.at(0);
    ui->lineEditOwnerAddr->setText(QString::fromStdString(ownerAddr.get_str()));

    // Load branchID
    json_spirit::Value branch = array.at(1);
    QString branchLabel = "Branch: ";
    QString branchID = QString::fromStdString(branch.get_str());
    branchLabel.append(branchID.left(12));
    branchLabel.append("...");
    ui->labelBranch->setText(branchLabel);
    if (branchID == "0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d") {
        ui->comboBoxBranch->setCurrentIndex(0); // Main
    } else if (branchID == "419cd87761f45c108a976ca6d93d4929c7c4d1ff4386f5089fc2f7ff7ae21ddf") {
        ui->comboBoxBranch->setCurrentIndex(1); // Sports
    } else if (branchID == "3277b5057ac9cda54e9edfbb45fd8bab38be1b5afc3cd6c587f6d17779f34f74") {
        ui->comboBoxBranch->setCurrentIndex(2); // Econ
    }

    // Load prompt
    json_spirit::Value prompt = array.at(2);
    ui->plainTextEditPrompt->setPlainText(QString::fromStdString(prompt.get_str()));

    // Load event over by
//    TODO: Replace with nLockTime
//    json_spirit::Value eventOverBy = array.at(3);
//    int overBy = eventOverBy.get_int();

    // Load voteMandatory
    json_spirit::Value voteMandatory = array.at(4);
    ui->checkBoxVoteMandatory->setChecked(voteMandatory.get_bool());

    // Load scaled
    json_spirit::Value scaled = array.at(5);
    bool isScaled = scaled.get_bool();
    ui->radioButtonScaled->setChecked(isScaled);
    if (isScaled) {
        // Load scale values
        json_spirit::Value scaledMin = array.at(6);
        ui->doubleSpinBoxScaledMin->setValue(scaledMin.get_real());
        json_spirit::Value scaledMax = array.at(7);
        ui->doubleSpinBoxScaledMax->setValue(scaledMax.get_real());
    }

    // Unhide the scaled spinboxes if the decision is scaled
    on_radioButtonScaled_clicked(isScaled);

    // Check for update index from model
    if (isScaled && array.size() == 10) {
        ui->pushButtonCreateDecision->hide();
        ui->pushButtonUpdateDecision->show();
        updateIndex = array.at(9).get_int();
    } else if (!isScaled && array.size() == 8) {
        ui->pushButtonCreateDecision->hide();
        ui->pushButtonUpdateDecision->show();
        updateIndex = array.at(7).get_int();
    }
}

void DecisionCreationWidget::on_pushButtonUpdateDecision_clicked()
{
    json_spirit::Array updatedDecision = createDecisionArray();
    if (!updatedDecision.empty() && updateIndex >= 0) {
        updatedDecision.push_back(updateIndex);
        emit updatedDecisionArray(updatedDecision);
    }
}

// Setup the UI for the combo creation widget
void DecisionCreationWidget::comboCreationUI()
{
    ui->pushButtonCreateDecision->hide();
}
