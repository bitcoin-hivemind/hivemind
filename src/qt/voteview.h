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

private:
    Ui::VoteView *ui;

    DecisionSelectionView *decisionSelection;
};

#endif // VOTEVIEW_H
