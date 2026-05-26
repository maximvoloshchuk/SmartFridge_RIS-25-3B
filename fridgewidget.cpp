#include "fridgewidget.h"
#include <QPainter>
#include <QPainterPath>

FridgeWidget::FridgeWidget(QWidget *parent)
    : QWidget(parent), m_blinkState(false)
{
    m_blinkTimer = new QTimer(this);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkState = !m_blinkState;
        update();
    });
    m_blinkTimer->start(500);
    setMinimumSize(380, 500);
}

void FridgeWidget::setProducts(const QList<Product> &products)
{
    m_products = products;
    update();
}

QRectF FridgeWidget::freezerRect() const
{
    qreal w = width() * 0.88;
    qreal h = height() * 0.28;
    qreal x = (width() - w) / 2;
    qreal y = 5;
    return QRectF(x, y, w, h);
}

QRectF FridgeWidget::fridgeRect() const
{
    qreal w = width() * 0.88;
    qreal h = height() * 0.65;
    qreal x = (width() - w) / 2;
    qreal y = height() * 0.30;
    return QRectF(x, y, w, h);
}

QList<QRectF> FridgeWidget::freezerShelves() const
{
    QList<QRectF> shelves;
    QRectF fz = freezerRect();
    qreal shelfH = (fz.height() - 10) / 2;
    for (int i = 0; i < 2; ++i) {
        shelves.append(QRectF(fz.x() + 8, fz.y() + 8 + i * shelfH,
                              fz.width() - 16, shelfH - 6));
    }
    return shelves;
}

QList<QRectF> FridgeWidget::fridgeShelves() const
{
    QList<QRectF> shelves;
    QRectF fr = fridgeRect();
    qreal shelfH = (fr.height() - 10) / 4;
    for (int i = 0; i < 4; ++i) {
        shelves.append(QRectF(fr.x() + 8, fr.y() + 8 + i * shelfH,
                              fr.width() - 16, shelfH - 6));
    }
    return shelves;
}

void FridgeWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#181825"));
    drawFridge(painter);
    drawProducts(painter);
}

void FridgeWidget::drawFridge(QPainter &painter)
{
    // === МОРОЗИЛКА ===
    QRectF fz = freezerRect();

    // Заголовок НАД морозилкой
    painter.setPen(QColor("#89dceb"));
    painter.setFont(QFont("Segoe UI", 10, QFont::Bold));
    QRectF fzTitleRect(fz.x(), fz.y() - 2, fz.width(), 18);
    painter.drawText(fzTitleRect, Qt::AlignHCenter | Qt::AlignTop, "❄️ МОРОЗИЛКА -18°C");

    // Корпус морозилки (ниже заголовка)
    QRectF fzBody(fz.x(), fz.y() + 18, fz.width(), fz.height() - 18);
    painter.setPen(QPen(QColor("#89b4fa"), 2));
    painter.setBrush(QColor("#11111b"));
    painter.drawRoundedRect(fzBody, 8, 8);

    // Полки морозилки
    painter.setPen(QPen(QColor("#45475a"), 1, Qt::DashLine));
    qreal fzShelfH = (fzBody.height() - 8) / 2;
    for (int i = 0; i < 2; ++i) {
        qreal sy = fzBody.y() + 4 + i * fzShelfH;
        painter.drawLine(QPointF(fzBody.x() + 10, sy), QPointF(fzBody.right() - 10, sy));
    }

    // === ХОЛОДИЛЬНИК ===
    QRectF fr = fridgeRect();

    // Заголовок НАД холодильником
    painter.setPen(QColor("#a6e3a1"));
    painter.setFont(QFont("Segoe UI", 10, QFont::Bold));
    QRectF frTitleRect(fr.x(), fr.y() - 2, fr.width(), 18);
    painter.drawText(frTitleRect, Qt::AlignHCenter | Qt::AlignTop, "🧊 ХОЛОДИЛЬНИК +4°C");

    // Корпус холодильника
    QRectF frBody(fr.x(), fr.y() + 18, fr.width(), fr.height() - 65);
    painter.setPen(QPen(QColor("#a6e3a1"), 2));
    painter.setBrush(QColor("#11111b"));
    painter.drawRoundedRect(frBody, 8, 8);

    // Полки холодильника
    painter.setPen(QPen(QColor("#45475a"), 1, Qt::DashLine));
    qreal frShelfH = (frBody.height() - 8) / 4;
    for (int i = 0; i < 4; ++i) {
        qreal sy = frBody.y() + 4 + i * frShelfH;
        painter.drawLine(QPointF(frBody.x() + 10, sy), QPointF(frBody.right() - 10, sy));
    }

    // Овощной отсек
    QRectF vegBox(fr.x() + 10, frBody.bottom() + 6, fr.width() - 20, 38);
    painter.setPen(QPen(QColor("#f9e2af"), 1));
    painter.setBrush(QColor("#1a1a2e"));
    painter.drawRoundedRect(vegBox, 5, 5);
    painter.setPen(QColor("#f9e2af"));
    painter.setFont(QFont("Segoe UI", 8));
    painter.drawText(vegBox, Qt::AlignCenter, "🥕 Овощной отсек +8°C");
}

void FridgeWidget::drawProducts(QPainter &painter)
{
    if (m_products.isEmpty()) return;

    auto fridgeShelvesList = fridgeShelves();
    auto freezerShelvesList = freezerShelves();

    int fridgeIndex = 0, freezerIndex = 0;
    qreal fridgeXOffset = 0, freezerXOffset = 0;

    for (const auto &p : m_products) {
        if (p.storageType() == "freezer" && freezerIndex < freezerShelvesList.size()) {
            QRectF shelf = freezerShelvesList[freezerIndex];
            qreal pw = qMin(80.0, shelf.width() / 3);
            qreal ph = shelf.height() * 0.75;

            if (freezerXOffset + pw > shelf.width()) {
                freezerXOffset = 0;
                freezerIndex++;
                if (freezerIndex >= freezerShelvesList.size()) continue;
                shelf = freezerShelvesList[freezerIndex];
            }

            QRectF productRect(shelf.x() + freezerXOffset + 4, shelf.center().y() - ph/2 + 2, pw, ph);
            QColor col = colorForProduct(p);
            painter.setBrush(col);
            painter.setPen(QPen(col.darker(130), 1));
            painter.drawRoundedRect(productRect, 5, 5);

            // Название продукта (полностью, если помещается)
            painter.setPen(QColor("#11111b"));
            painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
            QString label = p.name().left(12);
            if (pw < 70) label = p.name().left(8);
            painter.drawText(productRect.adjusted(2, 0, -2, 0), Qt::AlignCenter | Qt::TextWordWrap, label);

            freezerXOffset += pw + 6;
        }
        else if (fridgeIndex < fridgeShelvesList.size()) {
            QRectF shelf = fridgeShelvesList[fridgeIndex];
            qreal pw = qMin(85.0, shelf.width() / 3);
            qreal ph = shelf.height() * 0.75;

            if (fridgeXOffset + pw > shelf.width()) {
                fridgeXOffset = 0;
                fridgeIndex++;
                if (fridgeIndex >= fridgeShelvesList.size()) continue;
                shelf = fridgeShelvesList[fridgeIndex];
            }

            QRectF productRect(shelf.x() + fridgeXOffset + 4, shelf.center().y() - ph/2 + 2, pw, ph);
            QColor col = colorForProduct(p);
            painter.setBrush(col);
            painter.setPen(QPen(col.darker(130), 1));
            painter.drawRoundedRect(productRect, 5, 5);

            painter.setPen(QColor("#11111b"));
            painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
            QString label = p.name().left(12);
            if (pw < 75) label = p.name().left(8);
            painter.drawText(productRect.adjusted(2, 0, -2, 0), Qt::AlignCenter | Qt::TextWordWrap, label);

            fridgeXOffset += pw + 6;
        }
    }
}

QColor FridgeWidget::colorForProduct(const Product &p) const
{
    if (p.isExpired()) {
        return m_blinkState ? QColor("#f38ba8") : QColor("#e64553");
    }
    if (p.isExpiringSoon(3)) {
        return QColor("#f9e2af");
    }
    return QColor("#a6e3a1");
}

void FridgeWidget::resizeEvent(QResizeEvent *)
{
    update();
}
