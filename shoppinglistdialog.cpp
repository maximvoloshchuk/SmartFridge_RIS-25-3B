#include "shoppinglistdialog.h"
#include "ui_shoppinglistdialog.h"
#include "storagemanager.h"

ShoppingListDialog::ShoppingListDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ShoppingListDialog)
{
    ui->setupUi(this);
    generateShoppingList();
}

ShoppingListDialog::~ShoppingListDialog()
{
    delete ui;
}

void ShoppingListDialog::generateShoppingList()
{
    auto &storage = StorageManager::instance();

    // Избранное с низким запасом (≤2 шт)
    QList<Product> all = storage.getFridgeProducts(0);
    QList<Product> favLow;
    for (const auto &p : all) {
        if (p.isFavorite() && p.quantity() <= 2) favLow.append(p);
    }

    // Низкий запас (≤2 шт)
    QList<Product> lowStock;
    for (const auto &p : all) {
        if (p.quantity() <= 2) lowStock.append(p);
    }

    // Заканчивается срок (3 дня)
    QList<Product> expiring = storage.getFridgeExpiringSoon(0, 3);

    QString html = "<h2>🛒 Умный список покупок</h2><hr>";

    html += "<h3>⭐ Избранное (заканчивается):</h3><ul>";
    if (favLow.isEmpty()) {
        html += "<li>Всё в порядке!</li>";
    } else {
        for (const auto &p : favLow) {
            html += QString("<li><b>%1</b> — осталось %2 шт (категория: %3)</li>")
                        .arg(p.name()).arg(p.quantity()).arg(p.category());
        }
    }
    html += "</ul>";

    html += "<h3>📉 Низкий запас (все продукты):</h3><ul>";
    if (lowStock.isEmpty()) {
        html += "<li>Запасов достаточно!</li>";
    } else {
        for (const auto &p : lowStock) {
            html += QString("<li>%1 — %2 шт [%3]</li>")
                        .arg(p.name()).arg(p.quantity()).arg(p.category());
        }
    }
    html += "</ul>";

    html += "<h3>⏰ Скоро истекает срок:</h3><ul>";
    if (expiring.isEmpty()) {
        html += "<li>Все продукты свежие!</li>";
    } else {
        for (const auto &p : expiring) {
            html += QString("<li>%1 — %2 шт (осталось %3 дн.)</li>")
                        .arg(p.name()).arg(p.quantity()).arg(p.daysUntilExpired());
        }
    }
    html += "</ul>";

    html += "<hr><i>Список сгруппирован для удобства похода в магазин</i>";

    ui->textBrowser->setHtml(html);
}
