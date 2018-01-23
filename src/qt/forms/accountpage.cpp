#include "accountpage.h"
#include "ui_accountpage.h"
#include "walletmodel.h"
#include "wallet.h"
#include "accountmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "richtextdelegate.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "qrcodedialog.h"
#include <QClipboard>
#include <QListView>
#include <cstdlib>
#include "pandastyles.h"


AccountPage::AccountPage(CWallet* wallet_, QWidget *parent)
: QFrame(parent)
, ui(new Ui::AccountPage)
, model(NULL)
, wallet(wallet_)
, accountListModel(NULL)
{
    ui->setupUi(this);

    #ifndef USE_QRCODE
    delete ui->show_qrcode_button;
    ui->show_qrcode_button = NULL;
    #endif

    #ifdef USE_QRCODE
    ui->show_qrcode_button->setEnabled(false);
    #endif
    ui->copy_address_button->setEnabled(false);
    ui->sign_message_button->setEnabled(false);

    connect(ui->transaction_table->transactionView,SIGNAL(clicked(QModelIndex)),this,SLOT(transaction_table_cellClicked(QModelIndex)));
    connect(ui->account_filter_header,SIGNAL(exportClicked()),ui->transaction_table,SLOT(exportClicked()));
    connect(ui->CreateAccountBox, SIGNAL(cancelAccountCreation()), this, SLOT(cancelAccountCreation()));

    ui->last_30_days_in_bar->setMaximum(100);
    ui->last_30_days_out_bar->setMaximum(100);
}

void AccountPage::setModel(WalletModel *model)
{
    //fixme: delete signal connection if model already set at this point

    //fixme: LEAKLEAK
    RichTextDelegate* richTextDelegate = new RichTextDelegate(this);

    this->model = model;
    this->wallet = model->getWallet();
    delete accountListModel;
    if(model && model->getOptionsModel())
    {
        connect(model, SIGNAL(addressBookUpdated()), this, SLOT(update()));
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(update()));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(update()));

        accountListModel = new SingleColumnAccountModel(model->getMyAccountModel(),true,true);
        ui->TransactionTabAccountSelection->setModel(accountListModel);
        ui->TransactionTabAccountSelection->setItemDelegate(richTextDelegate);
        // Sadly the below is necessary in order to be able to style QComboBox pull down lists properly.
        ui->TransactionTabAccountSelection->setView(new QListView(this));

        ui->transaction_table->setModel(model);
        ui->account_filter_header->setModel(ui->transaction_table->transactionProxyModel);
        ui->CreateAccountBox->setModel(model);

        ui->account_summary_header->setModel(model);
    }
    update();
}

void AccountPage::setActiveAccount(const QString& accountName)
{
    if(accountName.isEmpty())
    {
        ui->TransactionTabAccountSelection->setCurrentIndex(0);
    }
    else
    {
        setSelectedAccountFromName(accountName);
    }
}

void AccountPage::setActivePane(int paneIndex)
{
    ui->tabWidget->setCurrentIndex(0);
}


AccountPage::~AccountPage()
{
    delete ui;
}

void AccountPage::on_TransactionTabAccountSelection_currentIndexChanged(int index)
{
    if(index == -1)
        return;

    if(ui->transaction_table->transactionProxyModel)
    {
        if(index==0)
        {
            model->getTransactionTableModel()->setCurrentAccountPrefix("");

            ui->transaction_table->transactionProxyModel->setAddressPrefix("");
            #ifdef USE_QRCODE
            ui->show_qrcode_button->setEnabled(false);
            #endif
            ui->copy_address_button->setEnabled(false);
            ui->sign_message_button->setEnabled(false);
        }
        else
        {
            model->getTransactionTableModel()->setCurrentAccountPrefix(model->getMyAccountModel()->data(1,index-1).toString());

            ui->transaction_table->transactionProxyModel->setAddressPrefix(model->getMyAccountModel()->data(1,index-1).toString());
            #ifdef USE_QRCODE
            ui->show_qrcode_button->setEnabled(true);
            #endif
            ui->copy_address_button->setEnabled(true);
            ui->sign_message_button->setEnabled(true);
        }
        update();
    }
}

void AccountPage::update()
{
    if (currentLoadState == LoadState_SyncHeadersFromEpoch)
        return;

    // Calculate various values used for widget display
    int selectionIndex = ui->TransactionTabAccountSelection->currentIndex();
    if(selectionIndex == -1)
        return;

    QString selectedAccountLabel;
    QString selectedAccountAddress;
    int64_t selectedAccountBalance = 0;
    int64_t selectedAccountAvailable = 0;
    int64_t selectedAccountEarningInterest = 0;
    int64_t selectedAccountPending = 0;
    qint64 allAccountEarningInterest = model->getStake();
    qint64 allAccountPending = model->getUnconfirmedBalance();
    qint64 allAccountBalance = model->getBalance();
    int unit = model->getOptionsModel()->getDisplayUnit();
    if(selectionIndex==0)
    {
        selectedAccountLabel=tr("All Accounts");
        selectedAccountAddress="";

        selectedAccountEarningInterest = allAccountEarningInterest;
        selectedAccountPending = allAccountPending;
        selectedAccountAvailable = allAccountBalance;
    }
    else
    {
        selectedAccountLabel=model->getMyAccountModel()->data(0,selectionIndex-1).toString();
        selectedAccountAddress=model->getMyAccountModel()->data(1,selectionIndex-1).toString();
        wallet->GetBalanceForAddress(selectedAccountAddress.toStdString(), selectedAccountEarningInterest, selectedAccountPending, selectedAccountAvailable);
    }
    selectedAccountBalance = selectedAccountEarningInterest + selectedAccountPending + selectedAccountAvailable;



    // Setup display values for 'account summary' header at top
    {
        ui->account_summary_header->update(selectedAccountLabel, selectedAccountAddress, formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(unit, selectedAccountBalance, true, false)), formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(unit, selectedAccountAvailable, true, false)), formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(unit, selectedAccountEarningInterest, true, false)), formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(unit, selectedAccountPending, true, false)),selectionIndex == 0 ? false : true);
    }


    // Setup display values for 'transaction total' header at bottom
    {
        int numTransactions = ui->transaction_table->transactionProxyModel->rowCount();
        if(numTransactions==1)
        {
            ui->num_transactions_found_footer->setText("1 "+tr("transactions found"));
        }
        else
        {
            ui->num_transactions_found_footer->setText(QString::number(numTransactions)+" "+tr("transactions found"));
        }
    }


    // Setup display values for 'in/out transaction summary' widget at top right
    {
        qint64 inTotal, outTotal;
        ui->transaction_table->transactionProxyModel->getLast30DaysInOut(inTotal, outTotal);
        outTotal = std::abs(outTotal);

        QString last30DaysInTotal = formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), inTotal, true, false));
        QString last30DaysOutTotal = formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), outTotal, true, false));

        ui->last_30_days_in_total->setText(last30DaysInTotal);
        ui->last_30_days_out_total->setText(last30DaysOutTotal);

        if(inTotal == 0 && outTotal == 0)
        {
            ui->last_30_days_in_bar->setValue(0);
            ui->last_30_days_out_bar->setValue(0);
        }
        else if(inTotal == outTotal)
        {
            ui->last_30_days_in_bar->setValue(50);
            ui->last_30_days_out_bar->setValue(50);
        }
        else
        {
            ui->last_30_days_in_bar->setValue(((double)inTotal/(inTotal+outTotal))*100);
            ui->last_30_days_out_bar->setValue(((double)outTotal/(inTotal+outTotal))*100);
        }
    }


    int64_t allAccountInterest = model->getTransactionTableModel()->getInterestGenerated();
    // Setup display values for 'interest summary' widget at bottom right
    if(wallet)
    {
        for(std::map<QLabel*,QLabel*>::iterator iter = mapInterestLabels.begin(); iter != mapInterestLabels.end(); iter++)
        {
            iter->first->deleteLater();
            iter->second->deleteLater();
        }
        mapInterestLabels.clear();

        for(int i = 0; i < model->getMyAccountModel()->rowCount(model->getMyAccountModel()->index(0,0)); i++)
        {
            QString accountName = model->getMyAccountModel()->data(0,i).toString().trimmed();
            QString accountAddress = model->getMyAccountModel()->data(1,i).toString().trimmed();
            int64_t accountInterest = model->getTransactionTableModel()->getInterestGenerated(accountAddress);
            QLabel* accountInterestLabel = new QLabel(accountName);
            QFont lblFont = accountInterestLabel->font();
            QString fontSize = TOTAL_FONT_SIZE;
            fontSize = fontSize.replace("px","");
            lblFont.setPixelSize(fontSize.toLong());
            QPalette lblPal = accountInterestLabel->palette();
            lblPal.setColor(QPalette::WindowText, QColor("#424242"));
            accountInterestLabel->setFont(lblFont);
            accountInterestLabel->setPalette(lblPal);
            QLabel* accountInterestValue = new QLabel(formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), accountInterest, true, false)));
            accountInterestValue->setFont(lblFont);
            accountInterestValue->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
            accountInterestValue->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            mapInterestLabels[accountInterestLabel] = accountInterestValue;
            ui->interest_form_layout->insertRow(0, accountInterestLabel, accountInterestValue);
        }
        ui->total_interest_value->setText(formatBitcoinAmountAsRichString(BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), allAccountInterest, true, false)));
    }
}


void AccountPage::on_sign_message_button_pressed()
{
    int selectionIndex = ui->TransactionTabAccountSelection->currentIndex();
    QString selectedAccountAddress;

    if(selectionIndex!=0)
    {
        selectedAccountAddress=model->getMyAccountModel()->data(1,selectionIndex-1).toString();
        emit signMessage(selectedAccountAddress);
    }
}

void AccountPage::on_show_qrcode_button_pressed()
{
    #ifdef USE_QRCODE
    int selectionIndex = ui->TransactionTabAccountSelection->currentIndex();
    QString selectedAccountAddress;
    QString selectedAccountLabel;

    if(selectionIndex!=0)
    {
        selectedAccountAddress=model->getMyAccountModel()->data(1,selectionIndex-1).toString();
        selectedAccountLabel=model->getMyAccountModel()->data(0,selectionIndex-1).toString();

        QRCodeDialog *dialog = new QRCodeDialog(selectedAccountAddress, selectedAccountLabel, true , this);
        dialog->setModel(model->getOptionsModel());
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
    #endif
}

void AccountPage::on_copy_address_button_pressed()
{
    int selectionIndex = ui->TransactionTabAccountSelection->currentIndex();
    if(selectionIndex!=0)
    {
        QApplication::clipboard()->setText(model->getMyAccountModel()->data(1,selectionIndex-1).toString());
    }
}

void AccountPage::transaction_table_cellClicked(const QModelIndex &index)
{
    //fixme: Hardcoded magic number, use proper column mapping
    //if(index.column() == TransactionTableModel::ToAddress || index.column() == TransactionTableModel::FromAddress || index.column() == TransactionTableModel::OurAddress || index.column() == TransactionTableModel::OtherAddress)
    if(index.column() == 3)
    {
        QString selectedAccountLabel = ui->transaction_table->transactionProxyModel->data(index).toString();
        setSelectedAccountFromName(selectedAccountLabel);
    }
}

void AccountPage::setSelectedAccountFromName(const QString &accountName)
{
    QString accountLabel=accountName.trimmed();
    for(int i = 0; i < model->getMyAccountModel()->rowCount(model->getMyAccountModel()->index(0,0)); i++)
    {
        QString accountCompare = model->getMyAccountModel()->data(0,i).toString();
        accountCompare=accountCompare.trimmed();
        if(accountCompare == accountLabel)
        {
            ui->TransactionTabAccountSelection->setCurrentIndex(i+1);
            return;
        }
    }
}

void AccountPage::cancelAccountCreation()
{
    ui->TransactionTabAccountSelection->setCurrentIndex(0);
    ui->tabWidget->setCurrentIndex(0);
}

void AccountPage::resizeEvent(QResizeEvent * e)
{
    setUpdatesEnabled(false);
    updateHeaderWidths();
    QFrame::resizeEvent(e);
    setUpdatesEnabled(true);
}

void AccountPage::showEvent(QShowEvent *e)
{
    QFrame::showEvent(e);
    updateHeaderWidths();
}

int countu=0;
// Link the width of the columns in out 'summary header' to the columns in the 'transaction table'.
void AccountPage::updateHeaderWidths()
{
    if(countu<2)
    {
        ++countu;
        //int balanceWidth = ui->transaction_table->getBalanceColumnWidth();
        //int amountWidth = ui->transaction_table->getAmountColumnWidth();
        //int accountWidth = ui->transaction_table->getAccountColumnWidth();
        //ui->account_summary_header->setColumnWidths(accountWidth, amountWidth, balanceWidth);
    }
}
