#include "storagemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

StorageManager::StorageManager() {}
StorageManager::~StorageManager() { if (m_db.isOpen()) m_db.close(); }

StorageManager& StorageManager::instance()
{
    static StorageManager instance;
    return instance;
}

bool StorageManager::initializeDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("smartfridge.db");
    if (!m_db.open()) { qCritical() << "DB error:" << m_db.lastError().text(); return false; }

    QSqlQuery q(m_db);

    bool ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS catalog (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            barcode TEXT,
            shelf_life_days INTEGER DEFAULT 7,
            category TEXT DEFAULT '',
            storage_type TEXT DEFAULT 'fridge',
            package_weight REAL DEFAULT 1,
            weight_unit TEXT DEFAULT 'шт',
            created_by INTEGER DEFAULT 0
        )
    )");
    if (!ok) { qCritical() << "catalog:" << q.lastError().text(); return false; }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS fridge (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER DEFAULT 0,
            product_name TEXT NOT NULL,
            barcode TEXT,
            manufacture_date TEXT NOT NULL,
            shelf_life_days INTEGER DEFAULT 7,
            category TEXT DEFAULT '',
            storage_type TEXT DEFAULT 'fridge',
            is_favorite INTEGER DEFAULT 0,
            quantity INTEGER DEFAULT 1,
            package_weight REAL DEFAULT 1,
            weight_unit TEXT DEFAULT 'шт'
        )
    )");
    if (!ok) { qCritical() << "fridge:" << q.lastError().text(); return false; }

    ok = q.exec(R"(
        CREATE TABLE IF NOT EXISTS consumption_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER DEFAULT 0,
            product_name TEXT NOT NULL,
            barcode TEXT,
            quantity INTEGER DEFAULT 1,
            timestamp TEXT NOT NULL
        )
    )");
    if (!ok) { qCritical() << "history:" << q.lastError().text(); return false; }

    q.exec("SELECT COUNT(*) FROM catalog");
    if (q.next() && q.value(0).toInt() == 0) seedCatalog();

    q.exec("SELECT COUNT(*) FROM fridge WHERE user_id=0");
    if (q.next() && q.value(0).toInt() == 0) seedFridge();

    return true;
}

// ==================== КАТАЛОГ ====================

QList<Product> StorageManager::getCatalogProducts(const QString &cf, qint64 userId) const
{
    QList<Product> list;
    QString sql = "SELECT * FROM catalog WHERE (created_by = 0 OR created_by = :uid)";
    if (!cf.isEmpty() && cf != "Все категории") sql += " AND category=:c";
    sql += " ORDER BY name ASC";
    QSqlQuery q(m_db); q.prepare(sql);
    q.bindValue(":uid", userId);
    if (!cf.isEmpty() && cf != "Все категории") q.bindValue(":c", cf);
    q.exec();
    while (q.next()) {
        Product p;
        p.setId(q.value("id").toInt());
        p.setName(q.value("name").toString());
        p.setBarcode(q.value("barcode").toString());
        p.setShelfLifeDays(q.value("shelf_life_days").toInt());
        p.setCategory(q.value("category").toString());
        p.setStorageType(q.value("storage_type").toString());
        p.setPackageWeight(q.value("package_weight").toDouble());
        p.setWeightUnit(q.value("weight_unit").toString());
        p.setQuantity(0);
        list.append(p);
    }
    return list;
}

// ==================== ХОЛОДИЛЬНИК ====================

Product StorageManager::rowToFridgeProduct(const QSqlQuery &q) const
{
    return Product(
        q.value("id").toInt(),
        q.value("product_name").toString(),
        q.value("barcode").toString(),
        QDate::fromString(q.value("manufacture_date").toString(), "yyyy-MM-dd"),
        q.value("shelf_life_days").toInt(),
        q.value("category").toString(),
        q.value("storage_type").toString(),
        q.value("is_favorite").toBool(),
        q.value("quantity").toInt(),
        q.value("package_weight").toDouble(),
        q.value("weight_unit").toString()
        );
}

Product StorageManager::getFridgeProduct(qint64 userId, int id) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM fridge WHERE id=:id AND user_id=:uid");
    q.bindValue(":id", id); q.bindValue(":uid", userId); q.exec();
    if (q.next()) return rowToFridgeProduct(q);
    return Product();
}

Product StorageManager::getFridgeProductByBarcode(qint64 userId, const QString &bc) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM fridge WHERE barcode=:b AND user_id=:uid");
    q.bindValue(":b", bc); q.bindValue(":uid", userId); q.exec();
    if (q.next()) return rowToFridgeProduct(q);
    return Product();
}

QList<Product> StorageManager::getFridgeProducts(qint64 userId, const QString &cf, bool fav) const
{
    QList<Product> list;
    QString sql = "SELECT * FROM fridge WHERE user_id=:uid";
    if (!cf.isEmpty() && cf != "Все категории") sql += " AND category=:c";
    if (fav) sql += " AND is_favorite=1";
    sql += " ORDER BY manufacture_date ASC";
    QSqlQuery q(m_db); q.prepare(sql); q.bindValue(":uid", userId);
    if (!cf.isEmpty() && cf != "Все категории") q.bindValue(":c", cf);
    q.exec();
    while (q.next()) list.append(rowToFridgeProduct(q));
    return list;
}

bool StorageManager::addToFridge(qint64 userId, const Product &p)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO fridge(user_id,product_name,barcode,manufacture_date,shelf_life_days,category,storage_type,is_favorite,quantity,package_weight,weight_unit) VALUES(:uid,:n,:b,:md,:sl,:c,:st,:f,:qty,:pw,:wu)");
    q.bindValue(":uid", userId); q.bindValue(":n", p.name()); q.bindValue(":b", p.barcode());
    q.bindValue(":md", p.manufactureDate().toString("yyyy-MM-dd"));
    q.bindValue(":sl", p.shelfLifeDays()); q.bindValue(":c", p.category());
    q.bindValue(":st", p.storageType()); q.bindValue(":f", p.isFavorite() ? 1 : 0);
    q.bindValue(":qty", p.quantity()); q.bindValue(":pw", p.packageWeight());
    q.bindValue(":wu", p.weightUnit());
    if (!q.exec()) { qWarning() << "addToFridge:" << q.lastError().text(); return false; }
    emit dataChanged(); return true;
}

bool StorageManager::updateFridgeProduct(qint64 userId, const Product &p)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE fridge SET product_name=:n,barcode=:b,manufacture_date=:md,shelf_life_days=:sl,category=:c,storage_type=:st,is_favorite=:f,quantity=:qty,package_weight=:pw,weight_unit=:wu WHERE id=:id AND user_id=:uid");
    q.bindValue(":n", p.name()); q.bindValue(":b", p.barcode());
    q.bindValue(":md", p.manufactureDate().toString("yyyy-MM-dd"));
    q.bindValue(":sl", p.shelfLifeDays()); q.bindValue(":c", p.category());
    q.bindValue(":st", p.storageType()); q.bindValue(":f", p.isFavorite() ? 1 : 0);
    q.bindValue(":qty", p.quantity()); q.bindValue(":pw", p.packageWeight());
    q.bindValue(":wu", p.weightUnit()); q.bindValue(":id", p.id()); q.bindValue(":uid", userId);
    if (!q.exec()) { qWarning() << "updateFridge:" << q.lastError().text(); return false; }
    emit dataChanged(); return true;
}

bool StorageManager::removeFromFridge(qint64 userId, int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM fridge WHERE id=:id AND user_id=:uid");
    q.bindValue(":id", id); q.bindValue(":uid", userId);
    if (!q.exec()) { qWarning() << "removeFridge:" << q.lastError().text(); return false; }
    emit dataChanged(); return true;
}

QList<Product> StorageManager::getFridgeExpiringSoon(qint64 userId, int days) const
{
    QList<Product> all = getFridgeProducts(userId), result;
    for (auto &p : all) if (p.isExpiringSoon(days) && !p.isExpired()) result.append(p);
    return result;
}

QList<Product> StorageManager::getFridgeExpired(qint64 userId) const
{
    QList<Product> all = getFridgeProducts(userId), result;
    for (auto &p : all) if (p.isExpired()) result.append(p);
    return result;
}

bool StorageManager::recordConsumption(qint64 userId, const Product &p, int quantity)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO consumption_history(user_id,product_name,barcode,quantity,timestamp) VALUES(:uid,:pn,:bc,:qty,:ts)");
    q.bindValue(":uid", userId);
    q.bindValue(":pn", p.name());
    q.bindValue(":bc", p.barcode());
    q.bindValue(":qty", quantity);
    q.bindValue(":ts", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    return q.exec();
}

QList<StorageManager::ConsumptionRecord> StorageManager::getConsumptionHistory(qint64 userId, int days) const
{
    QList<ConsumptionRecord> list;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM consumption_history WHERE user_id=:uid AND timestamp >= datetime('now', '-' || :d || ' days') ORDER BY timestamp DESC");
    q.bindValue(":uid", userId);
    q.bindValue(":d", days);
    q.exec();
    while (q.next()) {
        ConsumptionRecord r;
        r.productName = q.value("product_name").toString();
        r.quantity = q.value("quantity").toInt();
        r.timestamp = QDateTime::fromString(q.value("timestamp").toString(), "yyyy-MM-dd HH:mm:ss");
        list.append(r);
    }
    return list;
}

// ==================== ЗАПОЛНЕНИЕ ====================
bool StorageManager::addToCatalog(const Product &p, qint64 userId)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO catalog(name,barcode,shelf_life_days,category,storage_type,package_weight,weight_unit,created_by) VALUES(:n,:b,:sl,:c,:st,:pw,:wu,:cb)");
    q.bindValue(":n", p.name());
    q.bindValue(":b", p.barcode());
    q.bindValue(":sl", p.shelfLifeDays());
    q.bindValue(":c", p.category());
    q.bindValue(":st", p.storageType());
    q.bindValue(":pw", p.packageWeight());
    q.bindValue(":wu", p.weightUnit());
    q.bindValue(":cb", userId);

    if (!q.exec()) {
        qWarning() << "addToCatalog error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool StorageManager::removeLastConsumption(qint64 userId, const QString &barcode, int quantity)
{
    QSqlQuery q(m_db);
    // Удаляем последнюю запись с таким штрих-кодом и количеством
    q.prepare("DELETE FROM consumption_history WHERE id = ("
              "SELECT id FROM consumption_history "
              "WHERE user_id = :uid AND barcode = :bc AND quantity = :qty "
              "ORDER BY timestamp DESC LIMIT 1)");
    q.bindValue(":uid", userId);
    q.bindValue(":bc", barcode);
    q.bindValue(":qty", quantity);
    return q.exec();
}

void StorageManager::seedCatalog()
{
    auto add = [&](const QString &name, const QString &bc, int shelf, const QString &cat, const QString &st, double pw, const QString &wu) {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO catalog(name,barcode,shelf_life_days,category,storage_type,package_weight,weight_unit,created_by) VALUES(:n,:b,:sl,:c,:st,:pw,:wu,0)");
        q.bindValue(":n", name); q.bindValue(":b", bc); q.bindValue(":sl", shelf);
        q.bindValue(":c", cat); q.bindValue(":st", st); q.bindValue(":pw", pw); q.bindValue(":wu", wu);
        q.exec();
    };

    add("Молоко Простоквашино 2,5% 930мл", "4607053473544", 10, "Молочные продукты", "fridge", 930, "мл");
    add("Молоко сгущенное Рогачёвъ 380г", "4810065006079", 180, "Молочные продукты", "fridge", 380, "г");
    add("Сметана Простоквашино 20% 300г", "4600605029268", 30, "Молочные продукты", "fridge", 300, "г");
    add("Сыр Брест-Литовск Классический 200г", "4810268033261", 30, "Молочные продукты", "fridge", 200, "г");
    add("Творог Простоквашино 2% 200г", "4600605029343", 14, "Молочные продукты", "fridge", 200, "г");
    add("Кефир Простоквашино 2,5% 930г", "4607053473780", 14, "Молочные продукты", "fridge", 930, "г");
    add("Йогурт Активиа клубника 150г", "4607004890156", 10, "Молочные продукты", "fridge", 150, "г");
    add("Творог Нытвенский Обезжиренный 400г", "4620006671958", 14, "Молочные продукты", "fridge", 400, "г");
    add("Молоко Простоквашино 1,5% 930мл", "4690502002303", 14, "Молочные продукты", "fridge", 930, "мл");

    add("Сок апельсиновый Rich 1л", "4607004893001", 30, "Напитки", "fridge", 1.0, "л");
    add("Вода минеральная Боржоми 0.5л", "4607004893049", 365, "Напитки", "fridge", 0.5, "л");
    add("Квас Очаковский 1л", "4607004893056", 10, "Напитки", "fridge", 1.0, "л");
    add("Лимонад Черноголовка 0.5л", "4607004893063", 180, "Напитки", "fridge", 0.5, "л");
    add("Палпи Добрый 0.45л", "4607174577787", 7, "Напитки", "fridge", 0.45, "л");
    add("Cool Cola Zero 1.5л", "4600068052971", 90, "Напитки", "fridge", 1.5, "л");
    add("Сок яблочный Сады Придонья 1л", "4607004893018", 30, "Напитки", "fridge", 1.0, "л");

    add("Кетчуп Heinz 320г", "4601674084714", 180, "Соусы и специи", "fridge", 320, "г");
    add("Майонез Махеевъ 400мл", "4604248003081", 60, "Соусы и специи", "fridge", 400, "мл");
    add("Горчица Русская 140г", "4604248003517", 90, "Соусы и специи", "fridge", 140, "г");
    add("Соевый соус Kikkoman 150мл", "4607004895032", 120, "Соусы и специи", "fridge", 150, "мл");
    add("Чесночный соус Heinz 230г", "4601674009311", 45, "Соусы и специи", "fridge", 230, "г");
    add("Кетчуп Махеевъ 320г", "4604248020736", 120, "Соусы и специи", "fridge", 320, "г");
    add("Аджика Абхазская 200г", "4607004895056", 60, "Соусы и специи", "fridge", 200, "г");

    add("Пицца Пепперони 350г", "4607004894001", 90, "Заморозка", "freezer", 350, "г");
    add("Пельмени Сибирские 1кг", "4607004894025", 60, "Заморозка", "freezer", 1.0, "кг");
    add("Мороженое пломбир 500г", "4607004894018", 45, "Заморозка", "freezer", 500, "г");
    add("Овощная смесь 400г", "4607004894032", 60, "Заморозка", "freezer", 400, "г");
    add("Наггетсы куриные 300г", "4607004894049", 45, "Заморозка", "freezer", 300, "г");
    add("Блинчики с мясом 420г", "4607004894056", 50, "Заморозка", "freezer", 420, "г");
    add("Креветки Vici 500г", "4607004894063", 30, "Заморозка", "freezer", 500, "г");

    add("Куриное филе Петелинка 500г", "4607004891001", 5, "Мясо и птица", "fridge", 500, "г");
    add("Фарш Мираторг 400г", "4607004891018", 3, "Мясо и птица", "fridge", 400, "г");
    add("Сосиски Велком 350г", "4607004891025", 10, "Мясо и птица", "fridge", 350, "г");
    add("Колбаса Докторская 500г", "4607004891032", 15, "Мясо и птица", "fridge", 500, "г");
    add("Бекон Мираторг 150г", "4607004891049", 20, "Мясо и птица", "fridge", 150, "г");
    add("Крылья куриные 700г", "4607004891063", 4, "Мясо и птица", "fridge", 700, "г");
    add("Говядина тушёная 338г", "4607004891056", 180, "Мясо и птица", "fridge", 338, "г");

    add("Помидоры черри 250г", "4607004892001", 7, "Овощи и фрукты", "fridge", 250, "г");
    add("Огурцы гладкие 400г", "4607004892018", 5, "Овощи и фрукты", "fridge", 400, "г");
    add("Яблоки Гренни Смит 1кг", "4607004892025", 14, "Овощи и фрукты", "fridge", 1.0, "кг");
    add("Шампиньоны 300г", "4607004892049", 3, "Овощи и фрукты", "fridge", 300, "г");
    add("Зелень укроп 50г", "4607004892056", 2, "Овощи и фрукты", "fridge", 50, "г");
    add("Виноград кишмиш 500г", "4607004892063", 5, "Овощи и фрукты", "fridge", 500, "г");
    add("Апельсины 1кг", "4607004892032", 10, "Овощи и фрукты", "fridge", 1.0, "кг");

    add("Сайра 250г", "4607075851306", 120, "Морепродукты", "fridge", 250, "г");
    add("Окунь морской 1000г", "2535437011384", 3, "Морепродукты", "fridge", 1.0, "кг");
    add("Тунец в масле 185г", "4680019680026", 180, "Морепродукты", "fridge", 185, "г");
    add("Мидии в рассоле 270г", "4601132003172", 30, "Морепродукты", "fridge", 270, "г");
    add("Кальмар копчёный 150г", "4607095504534", 14, "Морепродукты", "fridge", 150, "г");
    add("Морская капуста 450г", "9607564075621", 45, "Морепродукты", "fridge", 450, "г");
    add("Икра Санта Бремор 180г", "4810168050085", 60, "Морепродукты", "fridge", 180, "г");
}

void StorageManager::seedFridge()
{
    QDate today = QDate::currentDate();
    auto add = [&](const QString &name, const QString &bc, int madeDaysAgo, int shelf, const QString &cat, const QString &st, bool fav, int qty, double pw, const QString &wu) {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO fridge(user_id,product_name,barcode,manufacture_date,shelf_life_days,category,storage_type,is_favorite,quantity,package_weight,weight_unit) VALUES(0,:n,:b,:md,:sl,:c,:st,:f,:qty,:pw,:wu)");
        q.bindValue(":n", name); q.bindValue(":b", bc);
        q.bindValue(":md", today.addDays(-madeDaysAgo).toString("yyyy-MM-dd"));
        q.bindValue(":sl", shelf); q.bindValue(":c", cat); q.bindValue(":st", st);
        q.bindValue(":f", fav ? 1 : 0); q.bindValue(":qty", qty); q.bindValue(":pw", pw); q.bindValue(":wu", wu);
        q.exec();
    };

    add("Молоко Простоквашино 2,5% 930мл", "4607053473544", 3, 10, "Молочные продукты", "fridge", true, 1, 930, "мл");
    add("Сыр Брест-Литовск Классический 200г", "4810268033261", 5, 30, "Молочные продукты", "fridge", true, 1, 200, "г");
    add("Куриное филе Петелинка 500г", "4607004891001", 1, 5, "Мясо и птица", "fridge", true, 1, 500, "г");
    add("Фарш Мираторг 400г", "4607004891018", 2, 3, "Мясо и птица", "fridge", false, 1, 400, "г");
    add("Пельмени Сибирские 1кг", "4607004894025", 10, 60, "Заморозка", "freezer", true, 1, 1.0, "кг");
}
