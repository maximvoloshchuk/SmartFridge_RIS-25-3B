#ifndef PRODUCT_H
#define PRODUCT_H

#include <QString>
#include <QDate>
#include <QJsonObject>

class Product
{
public:
    Product();
    Product(int id, const QString &name, const QString &barcode,
            const QDate &manufactureDate, int shelfLifeDays,
            const QString &category, const QString &storageType,
            bool isFavorite, int quantity,
            double packageWeight, const QString &weightUnit);

    // Геттеры
    int id() const;
    QString name() const;
    QString barcode() const;
    QDate manufactureDate() const;      // Дата изготовления
    int shelfLifeDays() const;          // Срок годности в днях (константа)
    QDate expirationDate() const;       // Вычисляется: manufactureDate + shelfLifeDays
    QString category() const;
    QString storageType() const;
    bool isFavorite() const;
    int quantity() const;               // Количество штук
    double packageWeight() const;       // Вес/объём одной упаковки (константа)
    QString weightUnit() const;         // г, мл, л, кг, шт
    QString packageStr() const;         // "400 г" или "1 л"
    QString quantityStr() const;        // "2 шт по 400 г"

    // Сеттеры
    void setId(int id);
    void setName(const QString &name);
    void setBarcode(const QString &barcode);
    void setManufactureDate(const QDate &date);
    void setShelfLifeDays(int days);
    void setCategory(const QString &category);
    void setStorageType(const QString &type);
    void setIsFavorite(bool fav);
    void setQuantity(int qty);
    void setPackageWeight(double weight);
    void setWeightUnit(const QString &unit);

    // Методы проверки
    int daysUntilExpired() const;
    bool isExpired() const;
    bool isExpiringSoon(int days = 3) const;
    bool canBeStoredInFridge() const;
    QString storageWarning() const;

    // Сериализация
    QJsonObject toJson() const;
    static Product fromJson(const QJsonObject &json);

private:
    int m_id;
    QString m_name;
    QString m_barcode;
    QDate m_manufactureDate;        // Дата изготовления
    int m_shelfLifeDays;            // Срок годности в днях
    QString m_category;
    QString m_storageType;
    bool m_isFavorite;
    int m_quantity;                 // Количество штук (целое!)
    double m_packageWeight;         // Вес/объём упаковки
    QString m_weightUnit;           // г, мл, л, кг, шт
};

#endif // PRODUCT_H
