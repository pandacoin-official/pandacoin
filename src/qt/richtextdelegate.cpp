#include "richtextdelegate.h"
#include "bitcoinunits.h"
#include "pandastyles.h"
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QPainter>
#include <QLabel>
#include <QPoint>

QString formatBitcoinAmountAsRichString(QString stringIn)
{
    stringIn = stringIn.replace(QRegExp("([0-9]+)[.]([0-9]+)"),"\\1.<span style='font-size:" + CURRENCY_DECIMAL_FONT_SIZE + ";'>\\2</span>");
    stringIn.replace(BitcoinUnits::name(BitcoinUnits::uBTC),"<span style='color: #8B8889; font-size:" + CURRENCY_DECIMAL_FONT_SIZE + ";'>" + BitcoinUnits::name(BitcoinUnits::uBTC)+ "</span>");
    stringIn.replace(BitcoinUnits::name(BitcoinUnits::mBTC),"<span style='color: #8B8889; font-size:" + CURRENCY_DECIMAL_FONT_SIZE + ";'>" + BitcoinUnits::name(BitcoinUnits::mBTC)+ "</span>");
    stringIn.replace(BitcoinUnits::name(BitcoinUnits::BTC),"<span style='color: #8B8889; font-size:" + CURRENCY_DECIMAL_FONT_SIZE + ";'>" + BitcoinUnits::name(BitcoinUnits::BTC)+ "</span>");

    return "<span style='font-size:"+CURRENCY_FONT_SIZE+";'>"+stringIn+"</span>";
}

RichTextDelegate::RichTextDelegate(QObject *parent, bool formatCurrency_)
: QStyledItemDelegate(parent)
, renderControl(new QLabel())
, formatCurrency(formatCurrency_)
, leftPadding(0)
{
}

RichTextDelegate::~RichTextDelegate()
{
    renderControl->deleteLater();
}

void RichTextDelegate::setLeftPadding(int leftPadding_)
{
    leftPadding = leftPadding_;
}

void RichTextDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem optionV4 = option;
    initStyleOption(&optionV4, index);

    QStyle *style = optionV4.widget? optionV4.widget->style() : QApplication::style();

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4);
    textRect.setHeight(textRect.height() - 1);

    // Painting original item without text.
    optionV4.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter);

    // First paint as normal so background etc. in place.
    QStyledItemDelegate::paint(painter, option, index);


    QVariant foregroundColor = index.data(Qt::ForegroundRole);
    if(foregroundColor.isValid() && foregroundColor.type() == QVariant::Color)
    {
        optionV4.palette.setColor(QPalette::WindowText, foregroundColor.value<QColor>());
    }
    QVariant backgroundColor = index.data(Qt::BackgroundRole);
    if(backgroundColor.isValid() && backgroundColor.type() == QVariant::Color)
    {
        optionV4.palette.setColor(QPalette::Background, backgroundColor.value<QColor>());
    }

    // Now paint rich text over.
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));

    // Blank out text with background color.
    painter->setBrush(QBrush(optionV4.palette.color(QPalette::Background)));
    painter->setPen(Qt::NoPen);
    painter->drawRect(0,0,textRect.width(),textRect.height());

    // Finally draw the actual text.
    renderControl->setPalette(optionV4.palette);
    renderControl->setAlignment(optionV4.displayAlignment);
    renderControl->setFont(optionV4.font);
    QString text;
    if(formatCurrency)
    {
        text = formatBitcoinAmountAsRichString(index.data().toString());
    }
    else
    {
        text = "<span style='font-size:"+CURRENCY_FONT_SIZE+";'>" + index.data().toString() + "</span>";
    }
    renderControl->setText(text);
    renderControl->setFixedSize(textRect.size());
    renderControl->render(painter, QPoint(leftPadding ,-1), QRegion());

    painter->restore();
}

QSize RichTextDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem optionV4 = option;
    initStyleOption(&optionV4, index);

    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setDefaultFont(optionV4.font);
    doc.setHtml(optionV4.text);
    doc.setTextWidth(optionV4.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}
