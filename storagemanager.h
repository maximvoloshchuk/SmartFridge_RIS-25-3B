#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QList>
#include <QDateTime>
#include "product.h"

class StorageManager : public QObject
{
    Q_OBJECT

public:
    static StorageManager& instance();

    bool initializeDatabase();

    // Каталог (общий список продуктов)
    QList<Product> getCatalogProducts(const QString &categoryFilter = "", qint64 userId = 0) const;
    bool addToCatalog(const Product &product, qint64 userId = 0);

    // Холодильник (единый для Qt и бота, user_id=0 для Qt, user_id=chatId для бота)
    Product getFridgeProduct(qint64 userId, int id) const;
    Product getFridgeProductByBarcode(qint64 userId, const QString &barcode) const;
    QList<Product> getFridgeProducts(qint64 userId, const QString &categoryFilter = "", bool favoritesOnly = false) const;
    bool addToFridge(qint64 userId, const Product &product);
    bool updateFridgeProduct(qint64 userId, const Product &product);
    bool removeFromFridge(qint64 userId, int id);
    QList<Product> getFridgeExpiringSoon(qint64 userId, int days = 3) const;
    QList<Product> getFridgeExpired(qint64 userId) const;

    struct ConsumptionRecord {
        QString productName;
        int quantity;
        QDateTime timestamp;
    };
    bool recordConsumption(qint64 userId, const Product &product, int quantity = 1);
    bool removeLastConsumption(qint64 userId, const QString &barcode, int quantity);
    QList<ConsumptionRecord> getConsumptionHistory(qint64 userId, int days = 7) const;

signals:
    void dataChanged();

private:
    StorageManager();
    ~StorageManager();
    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;

    QSqlDatabase m_db;
    void seedCatalog();
    void seedFridge();

    Product rowToFridgeProduct(const QSqlQuery &q) const;
};

#endif // STORAGEMANAGER_H
