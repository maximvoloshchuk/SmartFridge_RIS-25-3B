#include "product.h"
#include <QList>
#include <QMap>

Product::Product()
    : m_id(-1), m_shelfLifeDays(7), m_isFavorite(false),
    m_quantity(1), m_packageWeight(1), m_weightUnit("шт")
{
}

Product::Product(int id, const QString &name, const QString &barcode,
                 const QDate &manufactureDate, int shelfLifeDays,
                 const QString &category, const QString &storageType,
                 bool isFavorite, int quantity,
                 double packageWeight, const QString &weightUnit)
    : m_id(id), m_name(name), m_barcode(barcode),
    m_manufactureDate(manufactureDate), m_shelfLifeDays(shelfLifeDays),
    m_category(category), m_storageType(storageType),
    m_isFavorite(isFavorite), m_quantity(quantity),
    m_packageWeight(packageWeight), m_weightUnit(weightUnit)
{
}

// --- Геттеры ---
int Product::id() const { return m_id; }
QString Product::name() const { return m_name; }
QString Product::barcode() const { return m_barcode; }
QDate Product::manufactureDate() const { return m_manufactureDate; }
int Product::shelfLifeDays() const { return m_shelfLifeDays; }
QDate Product::expirationDate() const { return m_manufactureDate.addDays(m_shelfLifeDays); }
QString Product::category() const { return m_category; }
QString Product::storageType() const { return m_storageType; }
bool Product::isFavorite() const { return m_isFavorite; }
int Product::quantity() const { return m_quantity; }
double Product::packageWeight() const { return m_packageWeight; }
QString Product::weightUnit() const { return m_weightUnit; }

QString Product::packageStr() const
{
    if (m_weightUnit == "шт" && m_packageWeight == 1.0)
        return "1 шт";
    if (m_packageWeight == static_cast<int>(m_packageWeight))
        return QString::number(static_cast<int>(m_packageWeight)) + " " + m_weightUnit;
    return QString::number(m_packageWeight, 'f', 1) + " " + m_weightUnit;
}

QString Product::quantityStr() const
{
    return QString::number(m_quantity) + " шт по " + packageStr();
}

// --- Сеттеры ---
void Product::setId(int id) { m_id = id; }
void Product::setName(const QString &name) { m_name = name; }
void Product::setBarcode(const QString &barcode) { m_barcode = barcode; }
void Product::setManufactureDate(const QDate &date) { m_manufactureDate = date; }
void Product::setShelfLifeDays(int days) { m_shelfLifeDays = days; }
void Product::setCategory(const QString &category) { m_category = category; }
void Product::setStorageType(const QString &type) { m_storageType = type; }
void Product::setIsFavorite(bool fav) { m_isFavorite = fav; }
void Product::setQuantity(int qty) { m_quantity = qty; }
void Product::setPackageWeight(double weight) { m_packageWeight = weight; }
void Product::setWeightUnit(const QString &unit) { m_weightUnit = unit; }

int Product::daysUntilExpired() const
{
    return QDate::currentDate().daysTo(expirationDate());
}

bool Product::isExpired() const
{
    return daysUntilExpired() < 0;
}

bool Product::isExpiringSoon(int days) const
{
    int d = daysUntilExpired();
    return d >= 0 && d <= days;
}

bool Product::canBeStoredInFridge() const
{
    static QStringList forbiddenKeywords = {
        "мёд", "мед", "мука", "сахар", "соль", "крупа",
        "макароны", "спагетти", "рис", "гречка", "пшено",
        "хлеб", "батон", "булка",
        "картофель", "картошка", "лук", "чеснок",
        "банан", "ананас", "арбуз", "дыня",
        "шоколад", "конфеты", "печенье",
        "кофе", "чай",
        "консервы", "варенье",
        "масло оливковое", "масло подсолнечное", "масло растительное",
        "уксус", "соус соевый",
        "специи", "приправа", "перец молотый", "паприка",
        "сухофрукты", "орехи", "семечки",
        "мюсли", "хлопья", "сухой завтрак",
        "чипсы", "сухарики", "сушки", "баранки"
    };

    QString lowerName = m_name.toLower();
    for (const auto &kw : forbiddenKeywords) {
        if (lowerName.contains(kw)) return false;
    }
    return true;
}

QString Product::storageWarning() const
{
    static QMap<QString, QString> warnings = {
        {"мёд", "Мёд кристаллизуется в холодильнике."},
        {"мед", "Мёд кристаллизуется в холодильнике."},
        {"мука", "Мука впитывает влагу и запахи."},
        {"хлеб", "Хлеб в холодильнике черствеет быстрее!"},
        {"картофель", "Картофель превращает крахмал в сахар."},
        {"лук", "Лук размягчается и плесневеет."},
        {"банан", "Бананы чернеют и портятся."}
    };

    QString lowerName = m_name.toLower();
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (lowerName.contains(it.key())) {
            return "⚠️ " + it.value() + "\n\nЭтот продукт НЕЛЬЗЯ хранить в холодильнике!";
        }
    }

    if (m_storageType == "fridge" || m_storageType == "freezer")
        return "✅ Хранение в холодильнике допустимо";
    return "";
}

QJsonObject Product::toJson() const
{
    QJsonObject obj;
    obj["id"] = m_id;
    obj["name"] = m_name;
    obj["barcode"] = m_barcode;
    obj["manufactureDate"] = m_manufactureDate.toString("yyyy-MM-dd");
    obj["shelfLifeDays"] = m_shelfLifeDays;
    obj["category"] = m_category;
    obj["storageType"] = m_storageType;
    obj["isFavorite"] = m_isFavorite;
    obj["quantity"] = m_quantity;
    obj["packageWeight"] = m_packageWeight;
    obj["weightUnit"] = m_weightUnit;
    return obj;
}

Product Product::fromJson(const QJsonObject &json)
{
    return Product(
        json["id"].toInt(),
        json["name"].toString(),
        json["barcode"].toString(),
        QDate::fromString(json["manufactureDate"].toString(), "yyyy-MM-dd"),
        json["shelfLifeDays"].toInt(7),
        json["category"].toString(),
        json["storageType"].toString(),
        json["isFavorite"].toBool(),
        json["quantity"].toInt(1),
        json["packageWeight"].toDouble(1.0),
        json["weightUnit"].toString("шт")
        );
}
