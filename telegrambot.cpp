#include "telegrambot.h"
#include "storagemanager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>

TelegramBot::TelegramBot(const QString &botToken, QObject *parent)
    : QObject(parent)
    , m_botToken(botToken)
    , m_lastUpdateId(0)
    , m_running(false)
{
    m_apiUrl = QString("https://api.telegram.org/bot%1").arg(botToken);
    m_networkManager = new QNetworkAccessManager(this);
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &TelegramBot::checkUpdates);
}

TelegramBot::~TelegramBot()
{
    stop();
}

void TelegramBot::start()
{
    m_running = true;
    m_pollTimer->start(2000);
    emit botStatusChanged("✅ Telegram бот запущен");
}

void TelegramBot::stop()
{
    m_running = false;
    m_pollTimer->stop();
    emit botStatusChanged("⏸ Telegram бот остановлен");
}

// ==================== ОСНОВНОЙ ЦИКЛ ====================

void TelegramBot::checkUpdates()
{
    QUrl url(m_apiUrl + "/getUpdates");
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(m_lastUpdateId + 1));
    query.addQueryItem("timeout", "1");
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();
        if (!root["ok"].toBool()) return;

        QJsonArray updates = root["result"].toArray();
        for (const QJsonValue &val : updates) {
            QJsonObject update = val.toObject();
            m_lastUpdateId = update["update_id"].toInt();
            processUpdate(update);
        }
    });
}

void TelegramBot::processUpdate(const QJsonObject &update)
{
    if (!update.contains("message")) return;

    QJsonObject message = update["message"].toObject();
    QJsonObject chat = message["chat"].toObject();
    qint64 chatId = chat["id"].toVariant().toLongLong();

    // Проверяем, есть ли фото
    if (message.contains("photo")) {
        processPhoto(chatId, message);
        return;
    }

    QString text = message["text"].toString().trimmed();

    if (!m_userStates.contains(chatId)) {
        UserState s;
        s.chatId = chatId;
        s.state = "idle";
        m_userStates[chatId] = s;
    }

    UserState &state = m_userStates[chatId];

    // ==================== КОМАНДЫ ====================

    if (text == "/start") {
        state.state = "idle";
        sendMessage(chatId,
                    "🧊 *Привет! Я бот Умного Холодильника!*\n\n"
                    "📷 *Сканировать* — по штрих-коду\n"
                    "🔍 *Поиск* — найти по названию\n"
                    "📂 *Категории* — выбрать из категории\n"
                    "📦 *Холодильник* — посмотреть всё\n"
                    "📤 *Взять* — взять 1 шт продукта\n"
                    "🗑 *Удалить* — удалить продукт\n"
                    "📜 *История* — история потребления\n\n",
                    mainKeyboard());
        return;
    }

    if (text == "/help" || text == "❓ Помощь") {
        sendMessage(chatId,
                    "📖 *Способы добавления:*\n\n"
                    "1️⃣ 📷 Сканировать — отправьте штрих-код\n"
                    "2️⃣ 🔍 Поиск — введите часть названия\n"
                    "3️⃣ 📂 Категории — выберите категорию\n\n"
                    "Затем укажите количество и готово!");
        return;
    }

    if (text == "📷 Сканировать" || text == "/add") {
        state.state = "waiting_barcode";
        sendMessage(chatId, "📷 Отправьте штрих-код продукта:", cancelKeyboard());
        return;
    }

    if (text == "🔍 Поиск" || text == "/search") {
        state.state = "waiting_search";
        sendMessage(chatId, "🔍 Введите часть названия продукта (минимум 2 буквы):", cancelKeyboard());
        return;
    }

    if (text == "📂 Категории" || text == "/categories") {
        QStringList categories = {"Молочные продукты", "Мясо и птица", "Морепродукты",
                                  "Овощи и фрукты", "Напитки", "Заморозка", "Соусы и специи"};
        QString msg = "📂 *ВЫБЕРИТЕ КАТЕГОРИЮ:*\n\n";
        for (int i = 0; i < categories.size(); ++i)
            msg += QString("%1️⃣ %2\n").arg(i + 1).arg(categories[i]);
        msg += "\nВведите номер категории:";
        sendMessage(chatId, msg, cancelKeyboard());
        state.state = "waiting_category";
        return;
    }

    if (text == "🗑 Удалить" || text == "/delete") {
        auto products = StorageManager::instance().getFridgeProducts(chatId);
        if (products.isEmpty()) {
            sendMessage(chatId, "📭 Ваш холодильник пуст!", mainKeyboard());
            return;
        }
        QString msg = "🗑 *ВЫБЕРИТЕ НОМЕР ДЛЯ УДАЛЕНИЯ:*\n\n";
        for (int i = 0; i < products.size(); ++i)
            msg += QString("%1️⃣ %2 — %3\n").arg(i + 1).arg(products[i].name()).arg(products[i].quantityStr());
        msg += "\nВведите номер продукта:";
        sendMessage(chatId, msg, cancelKeyboard());
        state.state = "waiting_delete_number";
        return;
    }

    if (text == "📦 Холодильник") { showUserFridge(chatId); return; }
    if (text == "📊 Статистика") { showUserStats(chatId); return; }
    if (text == "📤 Взять") { startTakeProduct(chatId); return; }
    if (text == "📜 История") { showUserHistory(chatId); return; }


    if (text == "❌ Отмена") {
        state.state = "idle";
        sendMessage(chatId, "❌ Операция отменена.", mainKeyboard());
        return;
    }

    // ==================== ОБРАБОТКА СОСТОЯНИЙ ====================

    if (state.state == "waiting_barcode") {
        processBarcode(chatId, text);
    }
    else if (state.state == "waiting_quantity") {
        processQuantity(chatId, text);
    }
    else if (state.state == "waiting_take") {
        bool ok = false;
        int num = text.toInt(&ok);
        auto products = StorageManager::instance().getFridgeProducts(chatId);

        if (!ok || num < 1 || num > products.size()) {
            sendMessage(chatId, "❌ Введите правильный номер!", cancelKeyboard());
            return;
        }

        Product p = products[num - 1];

        // Если 1 шт — просто удаляем
        if (p.quantity() <= 1) {
            StorageManager::instance().recordConsumption(chatId, p, 1);
            StorageManager::instance().removeFromFridge(chatId, p.id());
            sendMessage(chatId, QString("📤 *Взято:* %1 (последняя шт)").arg(p.name()), mainKeyboard());
        } else {
            // Уменьшаем на 1
            p.setQuantity(p.quantity() - 1);
            StorageManager::instance().recordConsumption(chatId, p, 1);
            StorageManager::instance().updateFridgeProduct(chatId, p);
            sendMessage(chatId,
                        QString("📤 *Взято:* %1 (осталось %2 шт)").arg(p.name()).arg(p.quantity()),
                        mainKeyboard());
        }
        state.state = "idle";
        return;
    }

    else if (state.state == "waiting_delete_number") {
        bool ok = false;
        int num = text.toInt(&ok);
        auto products = StorageManager::instance().getFridgeProducts(chatId);
        if (!ok || num < 1 || num > products.size()) {
            sendMessage(chatId, "❌ Введите правильный номер!", cancelKeyboard());
            return;
        }
        Product toDelete = products[num - 1];
        StorageManager::instance().removeFromFridge(chatId, toDelete.id());
        sendMessage(chatId, QString("🗑 *Удалено:* %1").arg(toDelete.name()), mainKeyboard());
        state.state = "idle";
    }
    else if (state.state == "waiting_search") {
        QString searchText = text.trimmed();
        if (searchText.length() < 2) {
            sendMessage(chatId, "❌ Введите минимум 2 буквы!", cancelKeyboard());
            return;
        }
        auto catalog = StorageManager::instance().getCatalogProducts("", chatId);
        QList<Product> results;
        for (const auto &p : catalog)
            if (p.name().toLower().contains(searchText.toLower())) results.append(p);

        if (results.isEmpty()) {
            sendMessage(chatId, "❌ Ничего не найдено.", mainKeyboard());
            state.state = "idle";
            return;
        }
        if (results.size() == 1) {
            Product p = results.first();
            state.barcode = p.barcode();
            state.productName = p.name();
            state.productUnit = p.weightUnit();
            state.state = "waiting_quantity";
            sendMessage(chatId,
                        QString("✅ *%1*\n📂 %2 | ⚖ %3\n\nСколько кладёте?")
                            .arg(p.name(), p.category(), p.packageStr()),
                        cancelKeyboard());
            return;
        }
        QString msg = "🔍 *НАЙДЕНО:*\n\n";
        state.searchResults.clear();
        for (int i = 0; i < qMin(results.size(), 10); ++i) {
            msg += QString("%1️⃣ %2 — %3 [%4]\n")
                       .arg(i + 1).arg(results[i].name()).arg(results[i].packageStr()).arg(results[i].category());
            state.searchResults.append(results[i].name());
        }
        msg += "\nВведите номер:";
        sendMessage(chatId, msg, cancelKeyboard());
        state.state = "waiting_catalog_select";
    }
    else if (state.state == "waiting_category") {
        bool ok = false;
        int catNum = text.toInt(&ok);
        QStringList categories = {"Молочные продукты", "Мясо и птица", "Морепродукты",
                                  "Овощи и фрукты", "Напитки", "Заморозка", "Соусы и специи"};
        if (!ok || catNum < 1 || catNum > categories.size()) {
            sendMessage(chatId, "❌ Введите номер от 1 до " + QString::number(categories.size()), cancelKeyboard());
            return;
        }
        QString cat = categories[catNum - 1];
        auto catalog = StorageManager::instance().getCatalogProducts(cat, chatId);
        if (catalog.isEmpty()) {
            sendMessage(chatId, "❌ В категории '" + cat + "' нет продуктов.", mainKeyboard());
            state.state = "idle";
            return;
        }
        QString msg = "📂 *" + cat.toUpper() + ":*\n\n";
        state.searchResults.clear();
        for (int i = 0; i < qMin(catalog.size(), 15); ++i) {
            msg += QString("%1️⃣ %2 — %3\n").arg(i + 1).arg(catalog[i].name()).arg(catalog[i].packageStr());
            state.searchResults.append(catalog[i].name());
        }
        msg += "\nВведите номер продукта:";
        sendMessage(chatId, msg, cancelKeyboard());
        state.state = "waiting_catalog_select";
    }
    else if (state.state == "waiting_catalog_select") {
        bool ok = false;
        int num = text.toInt(&ok);
        if (!ok || num < 1 || num > state.searchResults.size()) {
            sendMessage(chatId, "❌ Введите правильный номер!", cancelKeyboard());
            return;
        }
        QString selectedName = state.searchResults[num - 1];
        auto catalog = StorageManager::instance().getCatalogProducts("", chatId);
        Product found;
        for (const auto &p : catalog)
            if (p.name() == selectedName) { found = p; break; }
        if (found.id() < 0) {
            sendMessage(chatId, "❌ Ошибка!", mainKeyboard());
            state.state = "idle";
            return;
        }
        state.barcode = found.barcode();
        state.productName = found.name();
        state.productUnit = found.weightUnit();
        state.state = "waiting_quantity";
        sendMessage(chatId,
                    QString("✅ *%1*\n📂 %2 | ⚖ %3\n\nСколько кладёте?")
                        .arg(found.name(), found.category(), found.packageStr()),
                    cancelKeyboard());
    }
    else if (state.state == "waiting_new_product_name") {
        if (text.toLower() == "нет" || text == "❌ Отмена") {
            state.state = "idle";
            sendMessage(chatId, "❌ Создание отменено.", mainKeyboard());
            return;
        }
        state.newProductName = text;
        state.state = "waiting_new_product_shelf";
        QTimer::singleShot(400, this, [this, chatId]() {
            sendMessage(chatId, "📆 Введите срок годности в днях (например: 7):", cancelKeyboard());
        });
        return;
    }
    else if (state.state == "waiting_new_product_shelf") {
        bool ok = false;
        int shelf = text.toInt(&ok);
        if (!ok || shelf <= 0) {
            sendMessage(chatId, "❌ Введите целое число!", cancelKeyboard());
            return;
        }
        state.newProductShelfLife = shelf;
        state.state = "waiting_new_product_category";

        QStringList cats = {"Молочные продукты", "Мясо и птица", "Морепродукты",
                            "Овощи и фрукты", "Напитки", "Заморозка", "Соусы и специи"};
        QString msg = "📂 Выберите категорию:\n";
        for (int i = 0; i < cats.size(); ++i)
            msg += QString("%1️⃣ %2\n").arg(i + 1).arg(cats[i]);
        QTimer::singleShot(400, this, [this, chatId, msg]() {
            sendMessage(chatId, msg, cancelKeyboard());
        });
        return;
    }
    else if (state.state == "waiting_new_product_category") {
        bool ok = false;
        int num = text.toInt(&ok);
        QStringList cats = {"Молочные продукты", "Мясо и птица", "Морепродукты",
                            "Овощи и фрукты", "Напитки", "Заморозка", "Соусы и специи"};
        if (!ok || num < 1 || num > cats.size()) {
            sendMessage(chatId, "❌ Введите номер от 1 до " + QString::number(cats.size()), cancelKeyboard());
            return;
        }
        state.newProductCategory = cats[num - 1];
        state.state = "waiting_new_product_weight";
        QTimer::singleShot(400, this, [this, chatId]() {
            sendMessage(chatId, "⚖ Введите вес упаковки (например: 400):", cancelKeyboard());
        });
        return;
    }
    else if (state.state == "waiting_new_product_weight") {
        bool ok = false;
        double weight = QString(text).replace(',', '.').toDouble(&ok);
        if (!ok || weight <= 0) {
            sendMessage(chatId, "❌ Введите положительное число!", cancelKeyboard());
            return;
        }
        state.newProductWeight = weight;
        state.state = "waiting_new_product_unit";
        QTimer::singleShot(400, this, [this, chatId]() {
            sendMessage(chatId, "📏 Выберите единицу:\n1️⃣ шт\n2️⃣ г\n3️⃣ кг\n4️⃣ мл\n5️⃣ л", cancelKeyboard());
        });
        return;
    }
    else if (state.state == "waiting_new_product_unit") {
        QStringList units = {"шт", "г", "кг", "мл", "л"};
        bool ok = false;
        int num = text.toInt(&ok);
        if (!ok || num < 1 || num > 5) {
            sendMessage(chatId, "❌ Введите номер от 1 до 5!", cancelKeyboard());
            return;
        }
        state.newProductWeightUnit = units[num - 1];
        state.state = "waiting_new_product_quantity";
        QTimer::singleShot(400, this, [this, chatId]() {
            sendMessage(chatId, "📦 Сколько штук кладёте?", cancelKeyboard());
        });
        return;
    }
    else if (state.state == "waiting_new_product_quantity") {
        bool ok = false;
        int qty = text.toInt(&ok);
        if (!ok || qty <= 0) {
            sendMessage(chatId, "❌ Введите целое число!", cancelKeyboard());
            return;
        }

        // Создаём продукт
        Product p;
        p.setName(state.newProductName);
        p.setBarcode(state.barcode);
        p.setShelfLifeDays(state.newProductShelfLife);
        p.setCategory(state.newProductCategory);
        p.setStorageType("fridge");
        p.setPackageWeight(state.newProductWeight);
        p.setWeightUnit(state.newProductWeightUnit);
        p.setQuantity(qty);
        p.setManufactureDate(QDate::currentDate());

        // Добавляем в каталог с ID пользователя
        StorageManager::instance().addToCatalog(p, chatId);
        // Добавляем в холодильник
        p.setId(-1);
        StorageManager::instance().addToFridge(chatId, p);

        sendMessage(chatId,
                    QString("✅ *Новый продукт создан!*\n%1 (%2 шт, %3 %4)\nДобавлен в каталог и холодильник!")
                        .arg(p.name()).arg(qty).arg(p.packageWeight()).arg(p.weightUnit()),
                    mainKeyboard());
        emit productAdded(p.name(), qty, p.weightUnit());
        state.state = "idle";
        return;
    }
}

// ==================== ДОБАВЛЕНИЕ ====================

void TelegramBot::processBarcode(qint64 chatId, const QString &barcode)
{
    UserState &state = m_userStates[chatId];
    auto catalog = StorageManager::instance().getCatalogProducts("", chatId);
    Product found;
    for (const auto &p : catalog)
        if (p.barcode() == barcode) { found = p; break; }

    if (found.id() < 0) {
        state.state = "waiting_new_product_name";
        state.barcode = barcode;
        QTimer::singleShot(400, this, [this, chatId, barcode]() {
            sendMessage(chatId,
                        QString("❌ Штрих-код `%1` не найден в каталоге.\n\nХотите создать новый продукт?\nВведите название (или 'нет' для отмены):").arg(barcode),
                        cancelKeyboard());
        });
        return;
    }
    state.barcode = barcode;
    state.productName = found.name();
    state.productUnit = found.weightUnit();
    state.state = "waiting_quantity";
    sendMessage(chatId,
                QString("✅ *%1*\n📂 %2 | ⚖ %3\n\nСколько кладёте?")
                    .arg(found.name(), found.category(), found.packageStr()),
                cancelKeyboard());
}

void TelegramBot::processQuantity(qint64 chatId, const QString &quantityText)
{
    UserState &state = m_userStates[chatId];
    bool ok = false;
    int quantity = QString(quantityText).replace(',', '.').toInt(&ok);
    if (!ok || quantity <= 0) {
        sendMessage(chatId, "❌ Введите целое число (штуки)!", cancelKeyboard());
        return;
    }

    auto catalog = StorageManager::instance().getCatalogProducts("", chatId);
    Product catalogProduct;
    for (const auto &p : catalog)
        if (p.barcode() == state.barcode) { catalogProduct = p; break; }

    if (catalogProduct.id() < 0) {
        sendMessage(chatId, "❌ Ошибка!", mainKeyboard());
        state.state = "idle";
        return;
    }

    Product existing = StorageManager::instance().getFridgeProductByBarcode(chatId, state.barcode);
    if (existing.id() > 0) {
        existing.setQuantity(existing.quantity() + quantity);
        StorageManager::instance().updateFridgeProduct(chatId, existing);
        sendMessage(chatId,
                    QString("✅ *Обновлено!*\n%1 теперь %2 шт").arg(existing.name()).arg(existing.quantity()),
                    mainKeyboard());
    } else {
        Product np = catalogProduct;
        np.setId(-1);
        np.setQuantity(quantity);
        np.setManufactureDate(QDate::currentDate());
        np.setWeightUnit(catalogProduct.weightUnit());
        np.setStorageType("fridge");
        if (StorageManager::instance().addToFridge(chatId, np)) {
            sendMessage(chatId,
                        QString("✅ *Добавлено!*\n%1 (%2 шт) в холодильнике 🧊").arg(np.name()).arg(quantity),
                        mainKeyboard());
            emit productAdded(np.name(), quantity, np.weightUnit());
        } else {
            sendMessage(chatId, "❌ Ошибка!", mainKeyboard());
        }
    }
    state.state = "idle";
}

// ==================== ПОКАЗ ====================

void TelegramBot::showUserFridge(qint64 chatId)
{
    auto products = StorageManager::instance().getFridgeProducts(chatId);
    if (products.isEmpty()) {
        sendMessage(chatId, "📭 Ваш холодильник пуст!");
        return;
    }
    QString text = "📦 *ВАШ ХОЛОДИЛЬНИК:*\n\n";
    for (const auto &p : products) {
        QString emoji = p.isExpired() ? "🔴" : (p.isExpiringSoon(3) ? "🟡" : "🟢");
        text += QString("%1 *%2* — %3 шт\n   📅 %4 дн. | ⚖ %5\n\n")
                    .arg(emoji, p.name()).arg(p.quantity())
                    .arg(p.daysUntilExpired()).arg(p.packageStr());
    }
    sendMessage(chatId, text);
}

void TelegramBot::showUserExpiringSoon(qint64 chatId)
{
    auto products = StorageManager::instance().getFridgeExpiringSoon(chatId, 3);
    if (products.isEmpty()) { sendMessage(chatId, "✅ Всё свежее!"); return; }
    QString text = "⚠️ *ЗАКАНЧИВАЕТСЯ:*\n\n";
    for (const auto &p : products)
        text += QString("🟡 %1 — %2 шт (осталось %3 дн.)\n").arg(p.name()).arg(p.quantity()).arg(p.daysUntilExpired());
    sendMessage(chatId, text);
}

void TelegramBot::showUserExpired(qint64 chatId)
{
    auto products = StorageManager::instance().getFridgeExpired(chatId);
    if (products.isEmpty()) { sendMessage(chatId, "✅ Нет просрочки!"); return; }
    QString text = "🚨 *ПРОСРОЧЕННЫЕ:*\n\n";
    for (const auto &p : products)
        text += QString("🔴 %1 — %2 шт (просрочен %3 дн. назад!)\n").arg(p.name()).arg(p.quantity()).arg(-p.daysUntilExpired());
    sendMessage(chatId, text);
}

void TelegramBot::showUserStats(qint64 chatId)
{
    auto all = StorageManager::instance().getFridgeProducts(chatId);
    auto expired = StorageManager::instance().getFridgeExpired(chatId);
    auto expiring = StorageManager::instance().getFridgeExpiringSoon(chatId, 3);
    QString text = "📊 *СТАТИСТИКА:*\n\n";
    text += QString("📦 Всего: %1\n🟢 Свежих: %2\n🟡 Заканчиваются: %3\n🔴 Просрочено: %4\n")
                .arg(all.size()).arg(all.size() - expired.size() - expiring.size()).arg(expiring.size()).arg(expired.size());
    sendMessage(chatId, text);
}

void TelegramBot::showUserHistory(qint64 chatId)
{
    auto history = StorageManager::instance().getConsumptionHistory(chatId, 7);

    if (history.isEmpty()) {
        sendMessage(chatId, "📭 Нет данных о потреблении за последние 7 дней.");
        return;
    }

    QMap<QString, int> counts;
    for (const auto &r : history)
        counts[r.productName] += r.quantity;

    QString text = "📜 *ИСТОРИЯ ПОТРЕБЛЕНИЯ (7 дн.):*\n\n";
    for (auto it = counts.begin(); it != counts.end(); ++it)
        text += QString("• %1 — %2 раз(а)\n").arg(it.key()).arg(it.value());

    text += QString("\n📦 Всего записей: %1").arg(history.size());

    sendMessage(chatId, text);
}

void TelegramBot::startTakeProduct(qint64 chatId)
{
    auto products = StorageManager::instance().getFridgeProducts(chatId);

    if (products.isEmpty()) {
        sendMessage(chatId, "📭 Ваш холодильник пуст!");
        return;
    }

    QString msg = "📤 *ВЫБЕРИТЕ НОМЕР ЧТОБЫ ВЗЯТЬ:*\n\n";
    for (int i = 0; i < products.size(); ++i) {
        msg += QString("%1️⃣ %2 — %3 шт\n")
                   .arg(i + 1)
                   .arg(products[i].name())
                   .arg(products[i].quantity());
    }
    msg += "\nВведите номер продукта:";

    sendMessage(chatId, msg, cancelKeyboard());

    UserState &state = m_userStates[chatId];
    state.state = "waiting_take";
}

void TelegramBot::processPhoto(qint64 chatId, const QJsonObject &message)
{
    QJsonArray photoArray = message["photo"].toArray();

    if (photoArray.isEmpty()) {
        sendMessage(chatId, "❌ Не удалось получить фото!");
        return;
    }

    // Берём самое большое фото (последнее в массиве)
    QJsonObject largestPhoto = photoArray.last().toObject();
    QString fileId = largestPhoto["file_id"].toString();

    // Получаем URL файла
    QNetworkRequest req(QUrl(m_apiUrl + "/getFile"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["file_id"] = fileId;

    QNetworkReply *fileReply = m_networkManager->post(req, QJsonDocument(body).toJson());

    connect(fileReply, &QNetworkReply::finished, this, [this, chatId, fileReply]() {
        fileReply->deleteLater();

        if (fileReply->error() != QNetworkReply::NoError) {
            sendMessage(chatId, "❌ Ошибка получения файла!");
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(fileReply->readAll());
        QJsonObject result = doc.object()["result"].toObject();
        QString filePath = result["file_path"].toString();

        if (filePath.isEmpty()) {
            sendMessage(chatId, "❌ Не удалось получить путь к файлу!");
            return;
        }

        // Скачиваем фото
        QUrl fileUrl("https://api.telegram.org/file/bot" + m_botToken + "/" + filePath);
        QNetworkRequest downloadReq(fileUrl);
        QNetworkReply *downloadReply = m_networkManager->get(downloadReq);

        connect(downloadReply, &QNetworkReply::finished, this, [this, chatId, downloadReply]() {
            downloadReply->deleteLater();

            if (downloadReply->error() != QNetworkReply::NoError) {
                sendMessage(chatId, "❌ Ошибка скачивания фото!");
                return;
            }

            QByteArray imageData = downloadReply->readAll();

            // Сохраняем фото в папку проекта
            QString tempPath = "C:/fridge_temp/barcode_" + QString::number(chatId) + ".jpg";
            QFile file(tempPath);

            if (file.open(QIODevice::WriteOnly)) {
                file.write(imageData);
                file.close();
            } else {
                sendMessage(chatId, "❌ Ошибка сохранения: " + file.errorString(), mainKeyboard());
                return;
            }

            sendMessage(chatId, "📷 Фото получено! Распознаю штрих-код...");

            // Запускаем Python для распознавания
            QProcess *process = new QProcess(this);
            QString scriptPath = "C:/Users/Пользователь/Desktop/SmartFridge/SmartFridge_RIS-25-3B-main/decode_barcode.py";

            process->setWorkingDirectory("C:/Users/Пользователь/Desktop/SmartFridge/SmartFridge_RIS-25-3B-main");
            QStringList args;
            args << scriptPath << tempPath;
            process->start("C:/Users/Пользователь/AppData/Local/Python/bin/python.exe", args);

            // Таймаут 10 секунд
            QTimer::singleShot(10000, process, [process, chatId, this]() {
                if (process->state() != QProcess::NotRunning) {
                    process->kill();
                    sendMessage(chatId, "❌ Превышено время ожидания!", mainKeyboard());
                }
            });

            connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, chatId, process, tempPath](int exitCode, QProcess::ExitStatus) {
                        QString result = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                        process->deleteLater();

                        QFile::remove(tempPath);

                        if (exitCode == 0 && !result.isEmpty() && result != "NOT_FOUND") {
                            sendMessage(chatId, "✅ Распознан штрих-код: `" + result + "`");

                            UserState &state = m_userStates[chatId];
                            state.barcode = result;
                            state.state = "waiting_quantity";

                            auto catalog = StorageManager::instance().getCatalogProducts("", chatId);
                            Product found;
                            for (const auto &p : catalog)
                                if (p.barcode() == result) { found = p; break; }

                            if (found.id() > 0) {
                                state.productName = found.name();
                                state.productUnit = found.weightUnit();
                                sendMessage(chatId,
                                            QString("✅ *%1*\n📂 %2\n\nСколько кладёте?")
                                                .arg(found.name(), found.category()),
                                            cancelKeyboard());
                            } else {
                                state.state = "waiting_new_product_name";
                                state.barcode = result;
                                QString bc = result;
                                QTimer::singleShot(400, this, [this, chatId, bc]() {
                                    sendMessage(chatId,
                                                QString("❌ Штрих-код `%1` не найден в каталоге.\n\nХотите создать новый продукт?\nВведите название (или 'нет' для отмены):").arg(bc),
                                                cancelKeyboard());
                                });
                            }
                        } else {
                            sendMessage(chatId, "❌ Штрих-код не распознан.", mainKeyboard());
                        }
                    });
        });
    });
}
// ==================== API ====================

void TelegramBot::sendMessage(qint64 chatId, const QString &text, const QJsonArray &replyMarkup)
{
    QNetworkRequest request(QUrl(m_apiUrl + "/sendMessage"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body;
    body["chat_id"] = chatId;
    body["text"] = text;
    body["parse_mode"] = "Markdown";
    if (!replyMarkup.isEmpty()) {
        QJsonObject kb;
        kb["keyboard"] = replyMarkup;
        kb["resize_keyboard"] = true;
        body["reply_markup"] = kb;
    }
    m_networkManager->post(request, QJsonDocument(body).toJson());
}

QJsonArray TelegramBot::mainKeyboard()
{
    QJsonArray r1, r2, r3, r4;

    QJsonObject b1; b1["text"] = "📷 Сканировать";
    QJsonObject b2; b2["text"] = "🔍 Поиск";
    r1.append(b1); r1.append(b2);

    QJsonObject b3; b3["text"] = "📂 Категории";
    r2.append(b3);

    QJsonObject b5; b5["text"] = "📦 Холодильник";
    QJsonObject b6; b6["text"] = "📤 Взять";
    QJsonObject b7; b7["text"] = "🗑 Удалить";
    r3.append(b5); r3.append(b6); r3.append(b7);

    QJsonObject b8; b8["text"] = "📊 Статистика";
    QJsonObject b9; b9["text"] = "📜 История";
    r4.append(b8); r4.append(b9);

    QJsonArray kb;
    kb.append(r1); kb.append(r2); kb.append(r3); kb.append(r4);
    return kb;
}

QJsonArray TelegramBot::cancelKeyboard()
{
    QJsonArray r;
    QJsonObject b; b["text"] = "❌ Отмена"; r.append(b);
    QJsonArray kb; kb.append(r);
    return kb;
}
