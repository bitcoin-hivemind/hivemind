// Copyright (c) 2015 The Hivemind Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
extern "C" {
#include "linalg/src/tc_mat.h"
}
#include "resolvevotecoltablemodel.h"
#include "resolvevotedialog.h"
#include "resolvevotegraph.h"
#include "resolvevoteinputtablemodel.h"
#include "resolvevoterowtablemodel.h"
#include "walletmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QString>
#include <QTableView>
#include <QTabWidget>
#include <QVBoxLayout>

ResolveVoteDialog::ResolveVoteDialog(QWidget *parent)
    : QDialog(parent),
    vote(0),
    model(0)
{
    /* starting data: start with minimum defualts */
    vote = tc_vote_ctr(2, 1);
    vote->NA = 0.138042e-30;
    vote->alpha = 0.10;
    vote->tol = 0.10;

    // oldrep
    double **rep = vote->rvecs[TC_VOTE_OLD_REP]->a;
    rep[0][0] = 0.5;
    rep[1][0] = 0.5;

    // Binary / scaled
    double **isbin = vote->cvecs[TC_VOTE_IS_BINARY]->a;
    for(uint32_t j=0; j < vote->nc; j++)
        isbin[0][j] = (j < 4)? 1.0: 0;

    // Vote matrix
    double **m = vote->M->a;
    m[0][0] = 1.0;
    m[1][0] = 1.0;

    // RC
    int rc = tc_vote_proc(vote);

    /* gui styling */
    setWindowTitle(tr("Resolve Vote Scenarios"));
    setMinimumSize(800, 200);

    QSplitter *splitter = new QSplitter(Qt::Vertical);

    // Vertical layout to contain input and output widgets
    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0,0,0,0);

    // Add input widget
    QWidget *inputWidget = new QWidget();
    splitter->addWidget(inputWidget);

    // Add output widget
    QWidget *outputWidget = new QWidget();
    splitter->addWidget(outputWidget);

    // Add splitter
    vlayout->addWidget(splitter);

    // Contain input widget in groupbox
    QVBoxLayout *vilayout = new QVBoxLayout(inputWidget);
    vilayout->setContentsMargins(0,0,0,0);
    QGroupBox *groupbox1 = new QGroupBox(tr("Input"));
    QVBoxLayout *vi1layout = new QVBoxLayout();
    groupbox1->setLayout(vi1layout);
    vilayout->addWidget(groupbox1);

    // Contain output widget in groupbox
    QVBoxLayout *volayout = new QVBoxLayout(outputWidget);
    volayout->setContentsMargins(0,0,0,0);
    QGroupBox *groupbox2 = new QGroupBox(tr("Output"));
    QVBoxLayout *vo1layout = new QVBoxLayout();
    groupbox2->setLayout(vo1layout);
    volayout->addWidget(groupbox2);

    /* vi1layout (input)       */

    /* input params */
    // {# voters}       [] {-}{+} {alpha}   []
    // {# Decisions}    [] {-}{+} {tol}     [] {NA} []
    QGridLayout *g1layout = new QGridLayout();
    g1layout->setHorizontalSpacing(5);
    g1layout->setVerticalSpacing(5);
    vi1layout->addLayout(g1layout);

    // #Voters value display
    nVotersLabel = new QLabel(tr("# Voters: "));
    g1layout->addWidget(nVotersLabel, /* row */0, /* col */0);
    nVotersLineEdit = new QLineEdit();
    nVotersLineEdit->setText(QString::number(vote->nr));
    nVotersLineEdit->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    g1layout->addWidget(nVotersLineEdit, /* row */0, /* col */1);
    connect(nVotersLineEdit, SIGNAL(editingFinished()), this, SLOT(onNVotersChange()));

    // #Voter - + buttons
    voterMinus = new QPushButton("-");
    voterMinus->setDefault(false);
    voterMinus->setAutoDefault(false);
    connect(voterMinus, SIGNAL(clicked()), this, SLOT(onVoterMinusClicked()));
    g1layout->addWidget(voterMinus, 0, 2);
    voterPlus = new QPushButton("+");
    voterPlus->setDefault(false);
    voterPlus->setAutoDefault(false);
    connect(voterPlus, SIGNAL(clicked()), this, SLOT(onVoterPlusClicked()));
    g1layout->addWidget(voterPlus, 0, 3);

    // #Decisions value display
    nDecisionsLabel = new QLabel(tr("# Decisions: "));
    g1layout->addWidget(nDecisionsLabel, /* row */1, /* col */0);
    nDecisionsLineEdit = new QLineEdit();
    nDecisionsLineEdit->setText(QString::number(vote->nc));
    nDecisionsLineEdit->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    g1layout->addWidget(nDecisionsLineEdit, /* row */1, /* col */1);
    connect(nDecisionsLineEdit, SIGNAL(editingFinished()), this, SLOT(onNDecisionsChange()));

    // #Decision - + buttons
    decisionMinus = new QPushButton("-");
    decisionMinus->setDefault(false);
    decisionMinus->setAutoDefault(false);
    connect(decisionMinus, SIGNAL(clicked()), this, SLOT(onDecisionMinusClicked()));
    g1layout->addWidget(decisionMinus, 1, 2);
    decisionPlus = new QPushButton("+");
    decisionPlus->setDefault(false);
    decisionPlus->setAutoDefault(false);
    connect(decisionPlus, SIGNAL(clicked()), this, SLOT(onDecisionPlusClicked()));
    g1layout->addWidget(decisionPlus, 1, 3);

    // Alpha value display
    alphaLabel = new QLabel(tr("alpha: "));
    g1layout->addWidget(alphaLabel, /* row */0, /* col */4);
    alphaLineEdit = new QLineEdit();
    alphaLineEdit->setText(QString::number(vote->alpha, 'f', 8));
    alphaLineEdit->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    g1layout->addWidget(alphaLineEdit, /* row */0, /* col */5);
    connect(alphaLineEdit, SIGNAL(editingFinished()), this, SLOT(onAlphaChange()));

    // Tolerance value display
    tolLabel = new QLabel(tr("tol: "));
    g1layout->addWidget(tolLabel, /* row */1, /* col */4);
    tolLineEdit = new QLineEdit();
    tolLineEdit->setText(QString::number(vote->tol, 'f', 8));
    tolLineEdit->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    g1layout->addWidget(tolLineEdit, /* row */1, /* col */5);
    connect(tolLineEdit, SIGNAL(editingFinished()), this, SLOT(onTolChange()));

    // NA value display
    NALabel = new QLabel(tr("NA: "));
    g1layout->addWidget(NALabel, /* row */1, /* col */6);
    NALineEdit = new QLineEdit();
    NALineEdit->setText(QString::number(vote->NA, 'e', 8));
    NALineEdit->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    g1layout->addWidget(NALineEdit, /* row */1, /* col */7);
    connect(NALineEdit, SIGNAL(textEdited()), this, SLOT(onNAChange()));

    /* input table */
    inputTableView = new QTableView();
    inputTableView->installEventFilter(this);
    inputTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    inputTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    vi1layout->addWidget(inputTableView);

    /* vo1layout (output)      */
    QTabWidget *tabs = new QTabWidget();
    QWidget *dataTab = new QWidget();
    tabs->addTab(dataTab, tr("Data"));
    QWidget *graphTab = new QWidget();
    tabs->addTab(graphTab, tr("Plot"));
    vo1layout->addWidget(tabs);

    /* graph tab */
    graphLayout = new QVBoxLayout(graphTab);
    resolveVoteGraph = new ResolveVoteGraph();
    resolveVoteGraph->setVotePtr((const struct tc_vote **)&vote);
    graphScrollArea = new QScrollArea();
    graphScrollArea->setWidgetResizable(true);
    graphScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    graphScrollArea->setWidget(resolveVoteGraph);
    graphLayout->addWidget(graphScrollArea);

    /* data tab */
    QVBoxLayout *vdlayout = new QVBoxLayout(dataTab);

    /* output params */
    QGridLayout *g2layout = new QGridLayout();
    g2layout->setHorizontalSpacing(5);
    g2layout->setColumnStretch(0, 1);
    g2layout->setColumnStretch(1, 1);
    g2layout->setColumnStretch(2, 9);
    vdlayout->addLayout(g2layout);
    voteProcRCLabel[0] = new QLabel(tr("rc: "));
    g2layout->addWidget(voteProcRCLabel[0], /* row */0, /* col */0);
    voteProcRCLabel[1] = new QLabel(QString::number(rc));
    g2layout->addWidget(voteProcRCLabel[1], /* row */0, /* col */1);

    /* col table */
    colTableView = new QTableView();
    colTableView->installEventFilter(this);
    colTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    colTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    vdlayout->addWidget(colTableView);

    /* row table */
    rowTableView = new QTableView();
    rowTableView->installEventFilter(this);
    rowTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    rowTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    vdlayout->addWidget(rowTableView);
}

ResolveVoteDialog::~ResolveVoteDialog()
{

}

void ResolveVoteDialog::setModel(WalletModel *model)
{
    if (!model)
        return;

    this->model = model;

    inputTableModel = model->getResolveVoteInputTableModel();
    if (inputTableModel) {
        inputTableModel->setResolveVoteDialog(this);
        inputTableModel->setVotePtr(&vote);
        inputTableView->setModel(inputTableModel);
        inputTableView->setAlternatingRowColors(true);
        inputTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

#if QT_VERSION >= 0x050000
        QHeaderView *hHeader = inputTableView->horizontalHeader();
        hHeader->sectionResizeMode(QHeaderView::Fixed);
        hHeader->setDefaultSectionSize(100);

        QHeaderView *vHeader = inputTableView->verticalHeader();
        vHeader->sectionResizeMode(QHeaderView::Fixed);
        vHeader->setDefaultSectionSize(20);
#endif
    }

    colTableModel = model->getResolveVoteColTableModel();
    if (colTableModel) {
        colTableModel->setVotePtr(&vote);
        colTableView->setModel(colTableModel);
        colTableView->setAlternatingRowColors(true);
        colTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

#if QT_VERSION >= 0x050000
        QHeaderView *hHeader = inputTableView->horizontalHeader();
        hHeader->sectionResizeMode(QHeaderView::Fixed);
        hHeader->setDefaultSectionSize(100);

        QHeaderView *vHeader = colTableView->verticalHeader();
        vHeader->sectionResizeMode(QHeaderView::Fixed);
        vHeader->setDefaultSectionSize(20);
#endif
    }

    rowTableModel = model->getResolveVoteRowTableModel();
    if (rowTableModel) {
        rowTableModel->setVotePtr(&vote);
        rowTableView->setModel(rowTableModel);
        rowTableView->setAlternatingRowColors(true);
        rowTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);

#if QT_VERSION >= 0x050000
        QHeaderView *hHeader = inputTableView->horizontalHeader();
        hHeader->sectionResizeMode(QHeaderView::Fixed);
        hHeader->setDefaultSectionSize(100);

        QHeaderView *vHeader = rowTableView->verticalHeader();
        vHeader->sectionResizeMode(QHeaderView::Fixed);
        vHeader->setDefaultSectionSize(20);
#endif
    }
}

bool ResolveVoteDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (!inputTableView || !rowTableView || !colTableView) {
        return QDialog::eventFilter(obj, event);
    }

    /* table with focus */
    QTableView *tableView = 0;
    if (obj == inputTableView)
        tableView = inputTableView;
    else
    if (obj == rowTableView)
        tableView = rowTableView;
    else
    if (obj == colTableView)
        tableView = colTableView;

    if (tableView)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if ((ke->key() == Qt::Key_C)
                && (ke->modifiers().testFlag(Qt::ControlModifier)))
            {
                /* Ctrl-C: copy from the selected cells */
                QString text;
                QItemSelectionModel *selection = tableView->selectionModel();
                QModelIndexList indexes = selection->selectedIndexes();
                int prev_row = -1;
                for(int i=0; i < indexes.size(); i++) {
                    QModelIndex index = indexes.at(i);
                    if (i) {
                        char c = (index.row() != prev_row)? '\n': '\t';
                        text.append(c);
                    }
                    QVariant data = tableView->model()->data(index);
                    text.append( data.toString() );
                    prev_row = index.row();
                }
                QApplication::clipboard()->setText(text);
                return true;
            }
            else
            if ((ke->key() == Qt::Key_V)
                && (ke->modifiers().testFlag(Qt::ControlModifier))
                && (tableView == inputTableView) /* inputTableView is modifiable */
                && (vote))
            {
                /* Ctrl-V: paste into the selected cells */
                QString text = QApplication::clipboard()->text();
                QStringList lines = text.split("\n", QString::SkipEmptyParts);
                QItemSelectionModel *selection = tableView->selectionModel();
                QModelIndexList indexes = selection->selectedIndexes();
                if (indexes.size() && indexes.at(0).isValid()) {
                    int row = indexes.at(0).row();
                    int col = indexes.at(0).column();
                    bool **isNA = new bool *[lines.size()];
                    double **dvalue = new double *[lines.size()];
                    int *num_fields = new int[lines.size()];
                    bool parse_error = false;
                    for(int i=0; i < lines.size() && !parse_error; i++) {
                        QStringList fields = lines[i].split("\t");
                        isNA[i] = new bool[fields.size()];
                        dvalue[i] = new double[fields.size()];
                        num_fields[i] = fields.size();
                        for(int j=0; j < fields.size() && !parse_error; j++) {
                            bool voterDecisionCell = col + j >= 1 && row + i >= 3;
                            if (fields[j] == "NA" && voterDecisionCell) {
                                isNA[i][j] = true;
                                dvalue[i][j] = 0.0;
                            }
                            else {
                                isNA[i][j] = false;
                                bool ok;
                                dvalue[i][j] = fields[j].toDouble(&ok);
                                if (!ok) {
                                    QMessageBox msgBox;
                                    if (voterDecisionCell) {
                                        msgBox.setText(tr("Enter a valid number or NA"));
                                    }
                                    else {
                                        msgBox.setText(tr("Enter a valid number"));
                                    }
                                    if (lines.size() > 1 || fields.size() > 1) {
                                        msgBox.setInformativeText(QString(tr(
                                                "The text '%1' in column %2, row %3 of "
                                                "the pasted data is not a valid value")).
                                                arg(fields[j]).arg(j + 1).arg(i + 1));
                                    }
                                    msgBox.exec();
                                    parse_error = true;
                                }
                            }
                        }
                    }

                    if (!parse_error) {
                        for(int i=0; i < lines.size(); i++) {
                            for(int j=0; j < num_fields[i]; j++) {
                                if (col + j >= (int)vote->nc + 1)
                                    break;

                                if (col + j == 0) { /* Old Rep */
                                    if ((row+i >= 3) && (row+i-3 < (int)vote->nr))
                                        vote->rvecs[TC_VOTE_OLD_REP]->a[row+i-3][0] = dvalue[i][j];
                                }
                                else
                                if (row + i == 0) /* Binary/Scalar */
                                    vote->cvecs[TC_VOTE_IS_BINARY]->a[0][col+j-1]
                                        = (dvalue[i][j] < 0.5)? 0.0: 1.0;
                                else
                                if (row + i == 1) /* Minimum */
                                   ;
                                else
                                if (row + i == 2) /* Maximum */
                                   ;
                                else
                                if (row + i < 3 + (int)vote->nr)
                                   vote->M->a[row+i-3][col+j-1] = (isNA[i][j])? vote->NA: dvalue[i][j];
                            }
                        }
                        inputTableModel->onDataChange();
                        onInputChange();
                    }
                    
                    for(int i=0; i < lines.size() && !parse_error; i++) {
                        delete[] isNA[i];
                        delete[] dvalue[i];
                    };
                    delete[] isNA;
                    delete[] dvalue;
                    delete[] num_fields;
                }
                return true;
            }
        }
    }

    return QDialog::eventFilter(obj, event);
}

void ResolveVoteDialog::onNVotersChange()
{
    if (!vote) {
        return;
    }

    unsigned int nVotersInput = nVotersLineEdit->text().toUInt();

    if ((nVotersInput <= 0) || (nVotersInput == vote->nr)) {
        /* bad input or no change. reset. */
        nVotersLineEdit->setText(QString::number(vote->nr));
    } else {
        nVotersLineEdit->setText(QString::number(nVotersInput));

        /* create new tc_vote */
        struct tc_vote *vote = tc_vote_ctr(nVotersInput, this->vote->nc);
        /* copy this->vote to vote */
        vote->NA = this->vote->NA;
        vote->alpha = this->vote->alpha;
        vote->tol = this->vote->tol;
        for(uint32_t i=0; i < vote->nr; i++)
            vote->rvecs[TC_VOTE_OLD_REP]->a[i][0]
                = (i < this->vote->nr)? this->vote->rvecs[TC_VOTE_OLD_REP]->a[i][0]: 0.0;
        for(uint32_t j=0; j < vote->nc; j++)
            vote->cvecs[TC_VOTE_IS_BINARY]->a[0][j]
                = (j < this->vote->nc)? this->vote->cvecs[TC_VOTE_IS_BINARY]->a[0][j]: 1.0;
        for(uint32_t i=0; i < vote->nr; i++)
            for(uint32_t j=0; j < vote->nc; j++)
                vote->M->a[i][j]
                    = ((i < this->vote->nr) && (j < this->vote->nc))? this->vote->M->a[i][j]: vote->NA;

        // Old vote matrix to be replaced with new vote matrix
        struct tc_vote *oldvote = this->vote;

        if (nVotersInput > oldvote->nr) {
            // Add rows
            inputTableModel->callBeginInsertRows(QModelIndex(), 3+oldvote->nr, 3+nVotersInput-1);
            rowTableModel->callBeginInsertRows(QModelIndex(), oldvote->nr, nVotersInput-1);
        } else {
            // Remove rows
            inputTableModel->callBeginRemoveRows(QModelIndex(), 3+nVotersInput, 3+oldvote->nr-1);
            rowTableModel->callBeginRemoveRows(QModelIndex(), nVotersInput, oldvote->nr-1);
        }

        // Replace old vote matrix with new vote matrix
        this->vote = vote;

        if (nVotersInput > oldvote->nr) {
            inputTableModel->callEndInsertRows();
            rowTableModel->callEndInsertRows();
        } else {
            inputTableModel->callEndRemoveRows();
            rowTableModel->callEndRemoveRows();
        }

        // Delete old vote matrix
        tc_vote_dtr(oldvote);

        /* recalc */
        onInputChange();
    }
}

void ResolveVoteDialog::onNDecisionsChange()
{
    char tmp[32];
    uint32_t nDecisions = atoi(nDecisionsLineEdit->text().toStdString().c_str());

    if ((nDecisions <= 0) || (nDecisions == vote->nc)) {
        /* bad input or no change. reset. */
        snprintf(tmp, sizeof(tmp), "%u", vote->nc);
        nDecisionsLineEdit->setText(QString(tmp));
    } else {
        snprintf(tmp, sizeof(tmp), "%u", nDecisions);
        nDecisionsLineEdit->setText(QString(tmp));

        /* create new tc_vote */
        struct tc_vote *vote = tc_vote_ctr(this->vote->nr, nDecisions);
        /* copy this->vote to vote */
        vote->NA = this->vote->NA;
        vote->alpha = this->vote->alpha;
        vote->tol = this->vote->tol;
        for(uint32_t i=0; i < vote->nr; i++)
            vote->rvecs[TC_VOTE_OLD_REP]->a[i][0]
                = (i < this->vote->nr)? this->vote->rvecs[TC_VOTE_OLD_REP]->a[i][0]: 0.0;
        for(uint32_t j=0; j < vote->nc; j++)
            vote->cvecs[TC_VOTE_IS_BINARY]->a[0][j]
                = (j < this->vote->nc)? this->vote->cvecs[TC_VOTE_IS_BINARY]->a[0][j]: 1.0;
        for(uint32_t i=0; i < vote->nr; i++)
            for(uint32_t j=0; j < vote->nc; j++)
                vote->M->a[i][j]
                    = ((i < this->vote->nr) && (j < this->vote->nc))? this->vote->M->a[i][j]: vote->NA;

        /* replace this->vote with vote */
        struct tc_vote *oldvote = this->vote;
        if (nDecisions > oldvote->nc) {
            inputTableModel->callBeginInsertColumns(QModelIndex(), oldvote->nc+1, nDecisions);
            colTableModel->callBeginInsertColumns(QModelIndex(), oldvote->nc, nDecisions-1);
        } else {
            inputTableModel->callBeginRemoveColumns(QModelIndex(), nDecisions+1, oldvote->nc);
            colTableModel->callBeginRemoveColumns(QModelIndex(), nDecisions, oldvote->nc-1);
        }
        this->vote = vote;
        if (nDecisions > oldvote->nc) {
            inputTableModel->callEndInsertColumns();
            colTableModel->callEndInsertColumns();
        } else {
            inputTableModel->callEndRemoveColumns();
            colTableModel->callEndRemoveColumns();
        }
        tc_vote_dtr(oldvote);

        /* recalc */
        onInputChange();
    }
}

void ResolveVoteDialog::onAlphaChange()
{
    char tmp[32];
    double alpha = atof(alphaLineEdit->text().toStdString().c_str());
    if ((alpha <= 0.0) || (alpha == vote->alpha)) {
        /* bad input or no change. reset. */
        snprintf(tmp, sizeof(tmp), "%.8f", vote->alpha);
        alphaLineEdit->setText(QString(tmp));
    } else {
        vote->alpha = alpha;
        snprintf(tmp, sizeof(tmp), "%.8f", vote->alpha);
        alphaLineEdit->setText(QString(tmp));

        /* recalc */
        onInputChange();
    }
}

void ResolveVoteDialog::onTolChange()
{
    char tmp[32];
    double tol = atof(tolLineEdit->text().toStdString().c_str());
    if ((tol <= 0.0) || (tol == vote->tol)) {
        /* bad input or no change. reset. */
        snprintf(tmp, sizeof(tmp), "%.8f", vote->tol);
        tolLineEdit->setText(QString(tmp));
    } else {
        vote->tol = tol;
        snprintf(tmp, sizeof(tmp), "%.8f", vote->tol);
        tolLineEdit->setText(QString(tmp));

        /* recalc */
        onInputChange();
    }
}

void ResolveVoteDialog::onNAChange()
{
    char tmp[32];
    double NA = atof(NALineEdit->text().toStdString().c_str());
    if ((NA <= 0.0) || (NA == vote->NA)) {
        /* bad input or no change. reset. */
        snprintf(tmp, sizeof(tmp), "%.8e", vote->NA);
        NALineEdit->setText(QString(tmp));
    } else {
        vote->NA = NA;
        snprintf(tmp, sizeof(tmp), "%.8e", vote->NA);
        NALineEdit->setText(QString(tmp));

        /* recalc */
        onInputChange();
    }
}

void ResolveVoteDialog::onInputChange(void)
{
    if (!vote) {
        return;
    }

    vote_proc_rc = tc_vote_proc(vote);
    voteProcRCLabel[1]->setText(QString::number(vote_proc_rc));
    colTableModel->onVoteChange(TC_VOTE_NCOLS, vote->nc);
    rowTableModel->onVoteChange(vote->nr, TC_VOTE_NROWS);

    graphLayout->removeWidget(graphScrollArea);
    delete graphScrollArea;

    resolveVoteGraph = new ResolveVoteGraph();
    resolveVoteGraph->setVotePtr((const struct tc_vote **)&vote);
    resolveVoteGraph->setMinimumHeight(2*20 + 50*vote->nc);
    graphScrollArea = new QScrollArea();
    graphScrollArea->setWidgetResizable(true);
    graphScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    graphScrollArea->setWidget(resolveVoteGraph);
    graphLayout->addWidget(graphScrollArea);
}

void ResolveVoteDialog::onVoterMinusClicked()
{
    unsigned int nVoters = this->nVotersLineEdit->text().toUInt();

    if (nVoters > 1) {
        nVoters--;
    }

    this->nVotersLineEdit->setText(QString::number(nVoters));
    onNVotersChange();
}

void ResolveVoteDialog::onVoterPlusClicked()
{
    unsigned int nVoters = this->nVotersLineEdit->text().toUInt();
    nVoters++;

    this->nVotersLineEdit->setText(QString::number(nVoters));
    onNVotersChange();
}

void ResolveVoteDialog::onDecisionMinusClicked()
{
    unsigned int nDecisions = this->nDecisionsLineEdit->text().toUInt();

    if (nDecisions > 0) {
        nDecisions--;
    }

    this->nDecisionsLineEdit->setText(QString::number(nDecisions));
    onNDecisionsChange();
}

void ResolveVoteDialog::onDecisionPlusClicked()
{
    unsigned int nDecisions = this->nDecisionsLineEdit->text().toUInt();
    nDecisions++;

    this->nDecisionsLineEdit->setText(QString::number(nDecisions));
    onNDecisionsChange();
}
