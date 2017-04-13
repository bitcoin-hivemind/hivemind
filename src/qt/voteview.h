/*
 * Copyright (c) 2017 The Hivemind Core developers
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 */
#ifndef VOTEVIEW_H
#define VOTEVIEW_H

#include <QWidget>

#include "decisionselectionview.h"
#include "json/json_spirit_writer_template.h"

namespace Ui {
class VoteView;
}

class VoteView : public QWidget
{
    Q_OBJECT

public:
    explicit VoteView(QWidget *parent = 0);
    ~VoteView();

private slots:
    void on_pushButtonSelectDecision_clicked();

    void decisionSelected(QString decisionHex);

    void on_pushButtonCreateRevealVote_clicked();

    void on_pushButtonCreateSealedVote_clicked();

    void selectDecision();

    void on_displayMessage(const QString &title, const QString &message);

signals:
    /** Signal raised to display messages from vote widget */
    void displayMessage(const QString &title, const QString &message);

private:
    Ui::VoteView *ui;

    DecisionSelectionView *decisionSelection;
};

#endif // VOTEVIEW_H
