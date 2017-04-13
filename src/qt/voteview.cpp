/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#include "voteview.h"
#include "ui_voteview.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QMessageBox>

#include <iostream>
#include "base58.h"
#include "txdb.h"
#include "wallet.h"

VoteView::VoteView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VoteView)
{
    ui->setupUi(this);

    connect(this, SIGNAL(displayMessage(QString,QString)),
            this, SLOT(on_displayMessage(QString,QString)));
}

VoteView::~VoteView()
{
    delete ui;
}

void VoteView::on_pushButtonSelectDecision_clicked()
{
    selectDecision();
}

void VoteView::decisionSelected(QString decisionHex)
{
    ui->lineEditDecisionID->setText(decisionHex);
}

void VoteView::on_pushButtonCreateRevealVote_clicked()
{
    extern json_spirit::Value createrevealvote(const json_spirit::Array &revealVoteParams, bool fHelp);

    // Grab the branch if it exists
    std::string branchID = "0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d";
    uint256 uBranch;
    uBranch.SetHex(branchID);
    marketBranch *branch = pmarkettree->GetBranch(uBranch);
    if (!branch) {
        emit displayMessage("Error creating Revealed Vote", "Invalid Branch ID, branch does not exist!");
        return;
    }

    // Get the voter's address
    CHivemindAddress voterAddress;
    voterAddress.is_votecoin = 1;
    voterAddress.SetString(ui->lineEditVotecoinAddress->text().toStdString());

    if (!voterAddress.IsValid()) {
        emit displayMessage("Error creating Reveald Vote", "Invalid Votecoin address!");
        return;
    }

    // Try to get the sealed vote
    uint256 uVote;
    uVote.SetHex(ui->lineEditSealedVoteID->text().toStdString());

    marketSealedVote *sealedVote = pmarkettree->GetSealedVote(uVote);
    if (!sealedVote) {
        emit displayMessage("Error creating Revealed Vote", "Vote does not exist!");
        return;
    }

    // Create spirit array of sealed vote params
    json_spirit::Array revealVoteParams;
    revealVoteParams.push_back(voterAddress.ToString());
    revealVoteParams.push_back(branch->GetHash().GetHex());
    int height = sealedVote->height;
    revealVoteParams.push_back(height);
    revealVoteParams.push_back(sealedVote->GetHash().GetHex());
    revealVoteParams.push_back(2016);
    QString decisionVote = ui->lineEditDecisionID->text();
    decisionVote.append(",");
    decisionVote.append(ui->comboBoxVote->currentIndex());
    revealVoteParams.push_back(decisionVote.toStdString());

    // Try to create the revealvote
    json_spirit::Value revealVoteResult;
    try {
        revealVoteResult = createrevealvote(revealVoteParams, false);
    } catch (const std::runtime_error &error) {
        QString errorText = QString::fromStdString(error.what());
        emit displayMessage("Error creating Revealed Vote", errorText);
        return;
    } catch (const std::exception &exception) {
        QString exceptionText = QString::fromStdString(exception.what());
        emit displayMessage("Error creating Revealed Vote", exceptionText);
        return;
    } catch (const json_spirit::Object &object) {
        revealVoteResult = object;
    }

    // Check the reveal vote result
    try {
        // Get reveal vote result pairs
        json_spirit::Object revealVoteResultObject = revealVoteResult.get_obj();
        json_spirit::Pair codePair = revealVoteResultObject[0];
        json_spirit::Pair messagePair = revealVoteResultObject[1];

        // If error, get error code and message
        if (codePair.name_ == "code") {
            int code = codePair.value_.get_int();
            if (code < 0) {
                QString errorText = QString::fromStdString(messagePair.value_.get_str());
                emit displayMessage("Error creating Revealed Vote", errorText);
                return;

            }
        }

        // If the reveal vote was created, display the txid and vote id
        QString resultMessage;
        resultMessage.append("Revealed vote has been submitted!\n");
        resultMessage.append(QString::fromStdString(codePair.name_));
        resultMessage.append(":\n");
        resultMessage.append(QString::fromStdString(codePair.value_.get_str()));
        resultMessage.append("\n\n");
        resultMessage.append(QString::fromStdString(messagePair.name_));
        resultMessage.append(":\n");
        resultMessage.append(QString::fromStdString(messagePair.value_.get_str()));
        emit displayMessage("Revealed Vote Submitted!", resultMessage);

    } catch (const std::runtime_error &error) {
        emit displayMessage("Error creating Revealed Vote!", QString::fromStdString(error.what()));

    } catch (const std::exception &exception) {
        emit displayMessage("Error creating Revealed Vote!", QString::fromStdString(exception.what()));
    }
}

void VoteView::on_pushButtonCreateSealedVote_clicked()
{
    /*
     * Creating votes goes as follows;
     * 1. Create reveal vote object.
     * 2. Create sealed vote object from revealed vote object.
     */
    extern json_spirit::Value createsealedvote(const json_spirit::Array &sealedVoteParams, bool fHelp);

    /* 1. Create reveal vote object */

    // Grab the branch if it exists
    std::string branchID = "0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d";
    uint256 uBranch;
    uBranch.SetHex(branchID);
    marketBranch *branch = pmarkettree->GetBranch(uBranch);
    if (!branch) {
        emit displayMessage("Error creating Sealed Vote", "Branch does not exist!");
        return;
    }

    // Create blank voteid
    uint256 uVote;

    // Grab the decision if it exists
    std::string decisionID = ui->lineEditDecisionID->text().toStdString();
    uint256 uDecision;
    uDecision.SetHex(decisionID);
    marketDecision *decision = pmarkettree->GetDecision(uDecision);
    if (!decision) {
        emit displayMessage("Error creating Sealed Vote", "Decision does not exist!");
        return;
    }

    // Get the voter's address
    CHivemindAddress voterAddress;
    voterAddress.is_votecoin = 1;
    voterAddress.SetString(ui->lineEditVotecoinAddress->text().toStdString());

    if (!voterAddress.IsValid()) {
        emit displayMessage("Error creating Sealed Vote", "Invalid Votecoin address!");
        return;
    }

    // Create revealvote
    marketRevealVote revealVote;
    revealVote.branchid.SetHex(branch->GetHash().GetHex());
    revealVote.height = ui->spinBoxHeight->value();
    revealVote.voteid = uVote;
    revealVote.decisionIDs.push_back(decision->GetHash());
    revealVote.decisionVotes.push_back(ui->comboBoxVote->currentIndex());
    revealVote.NA = 2016;
    voterAddress.GetKeyID(revealVote.keyID);

    /* 2. Create sealed vote object from revealed vote object */

    // Create spirit array of sealed vote params
    json_spirit::Array sealedVoteParams;
    sealedVoteParams.push_back(branch->GetHash().GetHex());
    sealedVoteParams.push_back(ui->spinBoxHeight->value());
    sealedVoteParams.push_back(revealVote.GetHash().GetHex());

    // Try to create the sealedvote
    json_spirit::Value sealedVoteResult;
    try {
        sealedVoteResult = createsealedvote(sealedVoteParams, false);
    } catch (const std::runtime_error &error) {
        emit displayMessage("Error creating Sealed Vote", QString::fromStdString(error.what()));
        return;
    } catch (const std::exception &exception) {
        emit displayMessage("Error creating Sealed Vote", QString::fromStdString(exception.what()));
        return;
    } catch (const json_spirit::Object &object) {
        sealedVoteResult = object;
    }

    // Check the sealed vote result
    try {
        // Get sealed vote result pairs
        json_spirit::Object sealedVoteResultObject = sealedVoteResult.get_obj();
        json_spirit::Pair codePair = sealedVoteResultObject[0];
        json_spirit::Pair messagePair = sealedVoteResultObject[1];

        // If error, get error code and message
        if (codePair.name_ == "code") {
            int code = codePair.value_.get_int();
            if (code < 0) {
                emit displayMessage("Error creating Sealed Vote", QString::fromStdString(messagePair.value_.get_str()));
                return;
            }
        }

        // If the sealed vote was created, display the txid and vote id
        QString resultMessage;
        resultMessage.append("Sealed vote has been submitted!\n");
        resultMessage.append(QString::fromStdString(codePair.name_));
        resultMessage.append(":\n");
        resultMessage.append(QString::fromStdString(codePair.value_.get_str()));
        resultMessage.append("\n\n");
        resultMessage.append(QString::fromStdString(messagePair.name_));
        resultMessage.append(":\n");
        resultMessage.append(QString::fromStdString(messagePair.value_.get_str()));
        emit displayMessage("Sealed Vote Submitted!", resultMessage);

    } catch (const std::runtime_error &error) {
        emit displayMessage("Error creating Sealed Vote", QString::fromStdString(error.what()));
    } catch (const std::exception &exception) {
        emit displayMessage("Error creating Sealed Vote", QString::fromStdString(exception.what()));
    }
}

void VoteView::selectDecision()
{
    // Grab the ID of the user selected branch
    QString branchID;
    if (ui->comboBoxBranch->currentText() == "Main") {
        branchID = "0f894a25c5e0318ee148fe54600ebbf50782f0a1df1eb2aab06321a8ccec270d";
    }

    // Exit if no branch is selected (technically impossible via the UI)
    if (branchID.isEmpty()) return;

    // Grab the branch
    uint256 uBranch;
    uBranch.SetHex(branchID.toStdString());
    const marketBranch *branch = pmarkettree->GetBranch(uBranch);

    if (!branch) return;

    // Grab decisions on the branch
    vector<marketDecision *> decisions = pmarkettree->GetDecisions(uBranch);

    // Setup the decision selection widget
    decisionSelection = new DecisionSelectionView(this);
    QVector<marketDecision *> qvDecisions = QVector<marketDecision *>::fromStdVector(decisions);
    QList<marketDecision *> qlDecisions = QList<marketDecision *>::fromVector(qvDecisions);
    decisionSelection->loadDecisions(qlDecisions);

    // Connect signals
    connect(decisionSelection, SIGNAL(decisionSelected(QString)),
            this, SLOT(decisionSelected(QString)));

    // Display the decision selection widget
    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(decisionSelection);
    QDialog *dialog = new QDialog(this);

    connect(decisionSelection, SIGNAL(done()),
            dialog, SLOT(close()));

    dialog->setWindowTitle("Select Decision");
    dialog->setLayout(hbox);
    dialog->exec();
}

void VoteView::on_displayMessage(const QString &title, const QString &message)
{
    QMessageBox messageBox;
    messageBox.setWindowTitle(title);
    messageBox.setText(message);
    messageBox.setStandardButtons(QMessageBox::Ok);
    messageBox.setDefaultButton(QMessageBox::Ok);
    messageBox.exec();
}
