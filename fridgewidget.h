#ifndef FRIDGEWIDGET_H
#define FRIDGEWIDGET_H

#include <QWidget>
#include <QTimer>
#include "product.h"

class FridgeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FridgeWidget(QWidget *parent = nullptr);
    void setProducts(const QList<Product> &products);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QList<Product> m_products;
    QTimer *m_blinkTimer;
    bool m_blinkState;

    QRectF freezerRect() const;
    QRectF fridgeRect() const;
    QList<QRectF> freezerShelves() const;
    QList<QRectF> fridgeShelves() const;

    void drawFridge(QPainter &painter);
    void drawProducts(QPainter &painter);
    QColor colorForProduct(const Product &p) const;
};

#endif // FRIDGEWIDGET_H
