#include "messagepage.h"
#include "ui_messagepage.h"

#include "messagemodel.h"
#include "bitcoingui.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>
#include <QAbstractItemDelegate>
#include <QPainter>

#include <QDebug>

#define DECORATION_SIZE 64
#define NUM_ITEMS 3

class MessageViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    MessageViewDelegate(): QAbstractItemDelegate()
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        //qDebug() << "Wtf" << index << index.data(MessageModel::ReceivedDateRole);

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));

        int ypad = 8;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        int align = (index.data(MessageModel::TypeRole) == 1 ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter;
        QRect amountRect (mainRect.left(), mainRect.top()+ypad,                       mainRect.width(), halfheight);
        QRect addressRect(mainRect.left(), mainRect.top()+ypad+halfheight,            mainRect.width(), halfheight);
        QRect messageRect(mainRect.left(), mainRect.top()+ypad+halfheight+halfheight, mainRect.width(), halfheight);

        icon.paint(painter, decorationRect);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(addressRect, align, index.data(MessageModel::FromAddressRole).toString());
        painter->drawText(amountRect,  align, GUIUtil::dateTimeStr(index.data(MessageModel::ReceivedDateRole).toDateTime()));
        painter->drawText(messageRect, align, index.data(MessageModel::MessageRole).toString());
        painter->restore();

        //QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        /*if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }*/
        //painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};

#include "messagepage.moc"

MessagePage::MessagePage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MessagePage),
    model(0),
    msgdelegate(new MessageViewDelegate())
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->deleteButton->setIcon(QIcon());
#endif

    // Context menu actions
    replyAction           = new QAction(ui->sendButton->text(),           this);
    copyFromAddressAction = new QAction(ui->copyFromAddressButton->text(), this);
    copyToAddressAction   = new QAction(ui->copyToAddressButton->text(),   this);
    deleteAction          = new QAction(ui->deleteButton->text(),          this);

    // Build context menu
    contextMenu = new QMenu();

    contextMenu->addAction(replyAction);
    contextMenu->addAction(copyFromAddressAction);
    contextMenu->addAction(copyToAddressAction);
    contextMenu->addAction(deleteAction);

    connect(replyAction,           SIGNAL(triggered()), this, SLOT(on_sendButton_clicked()));
    connect(copyFromAddressAction, SIGNAL(triggered()), this, SLOT(on_copyFromAddressButton_clicked()));
    connect(copyToAddressAction,   SIGNAL(triggered()), this, SLOT(on_copyToAddressButton_clicked()));
    connect(deleteAction,          SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    // Show Messages
    ui->listConversation->setItemDelegate(msgdelegate);
    ui->listConversation->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listConversation->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listConversation->setAttribute(Qt::WA_MacShowFocusRect, false);

    //connect(ui->listConversation, SIGNAL(clicked(QModelIndex)), this, SLOT(handleMessageClicked(QModelIndex)));
}

MessagePage::~MessagePage()
{
    delete ui;
}

void MessagePage::setModel(MessageModel *model)
{
    this->model = model;
    if(!model)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(2, Qt::DescendingOrder);

    // Set column widths
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Type,             100);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::Label,            100);
    ui->tableView->horizontalHeader()->setResizeMode(MessageModel::Label,            QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::FromAddress,      320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ToAddress,        320);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::SentDateTime,     170);
    ui->tableView->horizontalHeader()->resizeSection(MessageModel::ReceivedDateTime, 170);

    ui->messageEdit->setMaximumHeight(30);

    // Hidden columns
    ui->tableView->setColumnHidden(MessageModel::Message, true);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
            this, SLOT(selectionChanged()));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)),
            this, SLOT(selectionChanged()));

    selectionChanged();

    ui->listConversation->setModel(proxyModel);
    ui->listConversation->setModelColumn(MessageModel::ToAddress);
}

void MessagePage::on_sendButton_clicked()
{
    if(!model)
        return;

    if(!ui->tableView->selectionModel())
        return;

    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();

    if(indexes.isEmpty())
        return;

    /*
    SendMessagesDialog dlg(SendMessagesDialog::Encrypted, SendMessagesDialog::Dialog, this);

    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
    */
}

void MessagePage::on_copyFromAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::FromAddress, Qt::DisplayRole);
}

void MessagePage::on_copyToAddressButton_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, MessageModel::ToAddress, Qt::DisplayRole);
}

void MessagePage::on_deleteButton_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;
    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void MessagePage::on_backButton_clicked()
{
    ui->messageDetails->hide();
    ui->tableView->show();
    ui->newButton->setEnabled(true);
    ui->newButton->setVisible(true);
    ui->sendButton->setEnabled(false);
    ui->sendButton->setVisible(false);
    ui->messageEdit->setVisible(false);
    proxyModel->setFilterFixedString("");
}

void MessagePage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        replyAction->setEnabled(true);
        copyFromAddressAction->setEnabled(true);
        copyToAddressAction->setEnabled(true);
        deleteAction->setEnabled(true);

        ui->copyFromAddressButton->setEnabled(true);
        ui->copyToAddressButton->setEnabled(true);
        ui->deleteButton->setEnabled(true);

        ui->newButton->setEnabled(false);
        ui->newButton->setVisible(false);
        ui->sendButton->setEnabled(true);
        ui->sendButton->setVisible(true);
        ui->messageEdit->setVisible(true);

        ui->messageDetails->show();
        ui->tableView->hide();

        // Figure out which message was selected, and return it
        QModelIndexList messageColumn = table->selectionModel()->selectedRows(MessageModel::Message);
        QModelIndexList labelColumn   = table->selectionModel()->selectedRows(MessageModel::Label);
        QModelIndexList typeColumn    = table->selectionModel()->selectedRows(MessageModel::Type);

        //ui->listConversation->setModel(const_cast<QAbstractItemModel*>(table->selectionModel()->model()));
        //proxyModel->mapToSource();

        foreach (QModelIndex index, messageColumn)
        {
            ui->messageEdit->setPlainText(table->model()->data(index).toString());
        }

        /*
        foreach (QModelIndex index, typeColumn)
        {
            ui->labelType->setText(table->model()->data(index).toString());
        }*/

        foreach (QModelIndex index, labelColumn)
        {
            ui->contactLabel->setText(table->model()->data(index).toString());
        }

        proxyModel->setFilterRole(MessageModel::KeyRole);
        proxyModel->setFilterFixedString(table->selectionModel()->model()->data(table->selectionModel()->selectedRows(MessageModel::Key)[0], MessageModel::KeyRole).toString());
    }
    else
    {
        ui->newButton->setEnabled(true);
        ui->newButton->setVisible(true);
        ui->sendButton->setEnabled(false);
        ui->sendButton->setVisible(false);
        ui->copyFromAddressButton->setEnabled(false);
        ui->copyToAddressButton->setEnabled(false);
        ui->deleteButton->setEnabled(false);
        ui->messageEdit->hide();
        ui->messageDetails->hide();
        ui->messageEdit->clear();
    }
}

void MessagePage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Messages"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Type",             MessageModel::Type,             Qt::DisplayRole);
    writer.addColumn("Label",            MessageModel::Label,            Qt::DisplayRole);
    writer.addColumn("FromAddress",      MessageModel::FromAddress,      Qt::DisplayRole);
    writer.addColumn("ToAddress",        MessageModel::ToAddress,        Qt::DisplayRole);
    writer.addColumn("SentDateTime",     MessageModel::SentDateTime,     Qt::DisplayRole);
    writer.addColumn("ReceivedDateTime", MessageModel::ReceivedDateTime, Qt::DisplayRole);
    writer.addColumn("Message",          MessageModel::Message,          Qt::DisplayRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}


void MessagePage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

