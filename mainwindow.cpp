#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "addproductdialog.h"
#include "shoppinglistdialog.h"
#include "historychart.h"
#include "storagemanager.h"
#include "telegrambot.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QTimer>
#include <QCompleter>
#include <QStringListModel>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QVBoxLayout>
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_notificationManager(new NotificationManager(this))
    , m_showFavoritesOnly(false)
    , m_lastTakenQuantity(0)
    , m_hasLastTaken(false)
    , m_telegramBot(nullptr)
{
    ui->setupUi(this);

    QStringListModel *completerModel = new QStringListModel(this);
    QCompleter *completer = new QCompleter(this);
    completer->setModel(completerModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    ui->lineEditSearch->setCompleter(completer);

    auto updateCompleter = [completerModel]() {
        auto products = StorageManager::instance().getFridgeProducts(0);
        QStringList names;
        for (const auto &p : products) names << p.name();
        completerModel->setStringList(names);
    };

    connect(&StorageManager::instance(), &StorageManager::dataChanged,
            this, [this, updateCompleter]() {
                refreshTable(); refreshFridgeView(); updateStatusBar(); updateCompleter();
            });
    connect(m_notificationManager, &NotificationManager::newNotification,
            this, &MainWindow::onNotificationReceived);
    connect(ui->comboBoxCategory, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::applyFilter);
    connect(ui->lineEditSearch, &QLineEdit::textChanged,
            this, &MainWindow::on_lineEditSearch_textChanged);

    refreshTable(); refreshFridgeView(); updateStatusBar(); updateCompleter();
    m_notificationManager->startMonitoring(30000);
    showStatusMessage("✅ АРМ Умный Холодильник готов к работе.");

    QTimer::singleShot(1500, this, [this]() {
        auto expired = StorageManager::instance().getFridgeExpired(0);
        auto expiring = StorageManager::instance().getFridgeExpiringSoon(0, 3);
        if (!expired.isEmpty()) {
            QStringList names;
            for (const auto &p : expired) names << "• " + p.name();
            QMessageBox::warning(this, "🚨 ПРОСРОЧЕННЫЕ ПРОДУКТЫ!",
                                 "Следующие продукты испорчены:\n\n" + names.join("\n"));
        }
        if (!expiring.isEmpty()) {
            QStringList names;
            for (const auto &p : expiring)
                names << QString("• %1 — осталось %2 дн.").arg(p.name()).arg(p.daysUntilExpired());
            QMessageBox::information(this, "⏰ СРОК ГОДНОСТИ ИСТЕКАЕТ!",
                                     "Скоро испортятся:\n\n" + names.join("\n"));
        }
    });

    m_telegramBot = new TelegramBot("8759440296:AAEoChPIg764sQZ5H84UiCL2wVRHFn2FIQc");
    connect(m_telegramBot, &TelegramBot::productAdded, this,
            [this](const QString &name, double qty, const QString &unit) {
                showStatusMessage(QString("📱 Бот: %1 (%2 %3) добавлен!").arg(name).arg(qty).arg(unit));
            }, Qt::QueuedConnection);
    connect(m_telegramBot, &TelegramBot::botStatusChanged, this,
            [this](const QString &msg) { showStatusMessage(msg); }, Qt::QueuedConnection);
    m_telegramBot->start();
    startScannerServer();
}

MainWindow::~MainWindow()
{
    if (m_telegramBot) m_telegramBot->stop();
    delete ui;
}

void MainWindow::refreshTable()
{
    auto products = StorageManager::instance().getFridgeProducts(
        0, ui->comboBoxCategory->currentText(), m_showFavoritesOnly);
    QString searchText = ui->lineEditSearch->text().trimmed().toLower();
    if (!searchText.isEmpty()) {
        QList<Product> filtered;
        for (const auto &p : products) {
            if (p.name().toLower().contains(searchText) ||
                p.barcode().contains(searchText) ||
                p.category().toLower().contains(searchText))
                filtered.append(p);
        }
        products = filtered;
    }

    ui->tableProducts->setRowCount(0);
    ui->tableProducts->setRowCount(products.size());
    QColor bgColor("#313244"), textColor("#cdd6f4");

    for (int i = 0; i < products.size(); ++i) {
        const auto &p = products[i];
        auto *itemId = new QTableWidgetItem(QString::number(p.id()));
        auto *itemName = new QTableWidgetItem(p.name());
        auto *itemBarcode = new QTableWidgetItem(p.barcode());
        auto *itemCategory = new QTableWidgetItem(p.category());
        auto *itemExpDate = new QTableWidgetItem(p.expirationDate().toString("dd.MM.yyyy"));
        auto *itemDays = new QTableWidgetItem();
        auto *itemQty = new QTableWidgetItem(QString::number(p.quantity()) + " шт");
        auto *itemStorage = new QTableWidgetItem(
            p.storageType() == "freezer" ? "❄️ Морозилка" : "🧊 Холодильник");

        if (p.isExpired()) {
            itemDays->setText("🚨 ПРОСРОЧЕН!");
            itemDays->setBackground(QColor("#f38ba8"));
            itemDays->setForeground(QColor("#1e1e2e"));
        } else {
            itemDays->setText(QString::number(p.daysUntilExpired()) + " дн.");
            if (p.isExpiringSoon(3)) {
                itemDays->setBackground(QColor("#f9e2af"));
                itemDays->setForeground(QColor("#1e1e2e"));
            } else {
                itemDays->setBackground(bgColor);
                itemDays->setForeground(textColor);
            }
        }

        if (p.isFavorite()) { itemName->setText("⭐ " + p.name()); itemName->setForeground(QColor("#f9e2af")); }
        else { itemName->setForeground(textColor); }

        QList<QTableWidgetItem*> items = {itemId, itemName, itemBarcode, itemCategory,
                                           itemExpDate, itemQty, itemStorage};
        for (auto *item : items) item->setBackground(bgColor);
        itemId->setForeground(textColor); itemBarcode->setForeground(textColor);
        itemCategory->setForeground(textColor); itemExpDate->setForeground(textColor);
        itemQty->setForeground(textColor); itemStorage->setForeground(textColor);

        ui->tableProducts->setItem(i, 0, itemId); ui->tableProducts->setItem(i, 1, itemName);
        ui->tableProducts->setItem(i, 2, itemBarcode); ui->tableProducts->setItem(i, 3, itemCategory);
        ui->tableProducts->setItem(i, 4, itemExpDate); ui->tableProducts->setItem(i, 5, itemDays);
        ui->tableProducts->setItem(i, 6, itemQty); ui->tableProducts->setItem(i, 7, itemStorage);
    }
    ui->tableProducts->resizeColumnsToContents();
    ui->tableProducts->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::refreshFridgeView()
{
    auto all = StorageManager::instance().getFridgeProducts(0);
    QList<Product> fridge;
    for (const auto &p : all)
        if (p.storageType() == "fridge" || p.storageType() == "freezer") fridge.append(p);
    ui->fridgeWidget->setProducts(fridge);
}

void MainWindow::updateStatusBar()
{
    auto all = StorageManager::instance().getFridgeProducts(0);
    auto expired = StorageManager::instance().getFridgeExpired(0);
    ui->labelTotalInfo->setText(QString("📦 Всего: %1 | 🚨 Просрочено: %2").arg(all.size()).arg(expired.size()));
}

void MainWindow::showStatusMessage(const QString &msg, int timeout) { ui->statusbar->showMessage(msg, timeout); }
void MainWindow::applyFilter() { refreshTable(); refreshFridgeView(); }
void MainWindow::onNotificationReceived(const QString &msg) { showStatusMessage(msg, 8000); }

// ==================== ДОБАВИТЬ ====================
void MainWindow::on_btnAddProduct_clicked()
{
    QStringList options; options << "🔍 Выбрать из каталога" << "📷 Сканировать / Ввести штрих-код" << "✏️ Создать новый продукт";
    bool ok = false;
    QString choice = QInputDialog::getItem(this, "📦 Добавить продукт", "Как добавить?", options, 0, false, &ok);
    if (!ok) return;

    if (choice.contains("каталога")) {
        auto catalog = StorageManager::instance().getCatalogProducts("", 0);
        if (catalog.isEmpty()) { QMessageBox::information(this, "Пусто", "Каталог пуст."); return; }
        QStringList names; for (const auto &p : catalog) names << p.name();

        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle("🔍 Выберите продукт");
        dlg->setMinimumSize(500, 150);
        QVBoxLayout *lay = new QVBoxLayout(dlg);
        QLabel *lbl = new QLabel("🔍 Введите название:");
        QLineEdit *le = new QLineEdit(); le->setPlaceholderText("Молоко..."); le->setMinimumHeight(40);
        QCompleter *comp = new QCompleter(names, dlg);
        comp->setCaseSensitivity(Qt::CaseInsensitive); comp->setFilterMode(Qt::MatchContains);
        le->setCompleter(comp);
        QPushButton *btnOk = new QPushButton("✅ Выбрать"), *btnC = new QPushButton("❌ Отмена");
        QHBoxLayout *bl = new QHBoxLayout(); bl->addStretch(); bl->addWidget(btnOk); bl->addWidget(btnC);
        lay->addWidget(lbl); lay->addWidget(le); lay->addLayout(bl);
        connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);
        connect(btnC, &QPushButton::clicked, dlg, &QDialog::reject);
        connect(le, &QLineEdit::returnPressed, dlg, &QDialog::accept);
        if (dlg->exec() != QDialog::Accepted) { delete dlg; return; }
        QString typed = le->text().trimmed(); delete dlg;
        if (typed.isEmpty()) return;

        Product found;
        for (const auto &p : catalog) { if (p.name().toLower() == typed.toLower()) { found = p; break; } }
        if (found.id() < 0) {
            QList<Product> matches;
            for (const auto &p : catalog) if (p.name().toLower().contains(typed.toLower())) matches.append(p);
            if (matches.isEmpty()) { QMessageBox::warning(this, "Не найдено", "Не найден: " + typed); return; }
            if (matches.size() == 1) found = matches.first();
            else {
                QStringList mnames; for (const auto &p : matches) mnames << p.name();
                QString sel = QInputDialog::getItem(this, "Уточните", "Несколько:", mnames, 0, false, &ok);
                if (!ok) return;
                for (const auto &p : matches) if (p.name() == sel) { found = p; break; }
            }
        }
        if (found.id() < 0) return;

        Product exist = StorageManager::instance().getFridgeProductByBarcode(0, found.barcode());
        if (exist.id() > 0) {
            int addQty = QInputDialog::getInt(this, "Уже есть",
                                              QString("'%1' уже есть (%2 шт). Сколько добавить?").arg(found.name()).arg(exist.quantity()),
                                              1, 1, 999, 1, &ok);
            if (ok) { exist.setQuantity(exist.quantity() + addQty); StorageManager::instance().updateFridgeProduct(0, exist); }
            return;
        }

        int qty = QInputDialog::getInt(this, "Количество",
                                       QString("Сколько '%1'?").arg(found.name()), 1, 1, 999, 1, &ok);
        if (!ok) return;

        bool dateOk = false;
        QString dateStr = QInputDialog::getText(this, "📅 Дата изготовления",
                                                QString("Введите дату изготовления '%1'\nСрок годности: %2 дн.\nФормат: ГГГГ-ММ-ДД")
                                                    .arg(found.name()).arg(found.shelfLifeDays()),
                                                QLineEdit::Normal, QDate::currentDate().toString("yyyy-MM-dd"), &dateOk);

        if (!dateOk || dateStr.isEmpty()) return;

        QDate manDate = QDate::fromString(dateStr.trimmed(), "yyyy-MM-dd");
        if (!manDate.isValid()) {
            QMessageBox::warning(this, "Ошибка", "Неверный формат даты! (ГГГГ-ММ-ДД)");
            return;
        }

        Product np = found;
        np.setId(-1);
        np.setQuantity(qty);
        np.setManufactureDate(manDate);
        StorageManager::instance().addToFridge(0, np);
        showStatusMessage("✅ '" + np.name() + "' добавлен!");
    }

    else if (choice.contains("Создать новый")) {
    AddProductDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        Product p = dialog.getProduct();

        // Проверка: есть ли уже такой штрих-код в каталоге
        auto catalog = StorageManager::instance().getCatalogProducts("", 0);
        bool barcodeExists = false;
        Product existingBarcode;
        for (const auto &cp : catalog) {
            if (cp.barcode() == p.barcode() && !p.barcode().isEmpty()) {
                barcodeExists = true;
                existingBarcode = cp;
                break;
            }
        }

        if (barcodeExists) {
            int ret = QMessageBox::question(this, "Штрих-код уже существует",
                                            QString("Продукт '%1' с таким штрих-кодом уже есть в каталоге.\nДобавить существующий?")
                                                .arg(existingBarcode.name()),
                                            QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                int qty = QInputDialog::getInt(this, "Количество",
                                               QString("Сколько '%1'?").arg(existingBarcode.name()), 1, 1, 999, 1, &ok);
                if (ok) {
                    existingBarcode.setId(-1);
                    existingBarcode.setQuantity(qty);
                    existingBarcode.setManufactureDate(QDate::currentDate());
                    StorageManager::instance().addToFridge(0, existingBarcode);
                    showStatusMessage("✅ '" + existingBarcode.name() + "' добавлен!");
                }
            }
            return;
        }

        // Проверка: есть ли продукт с таким же названием
        bool nameExists = false;
        Product existingName;
        for (const auto &cp : catalog) {
            if (cp.name().toLower() == p.name().toLower()) {
                nameExists = true;
                existingName = cp;
                break;
            }
        }

        if (nameExists) {
            int ret = QMessageBox::question(this, "Похожий продукт есть",
                                            QString("Продукт '%1' уже есть в каталоге.\nДобавить существующий?")
                                                .arg(existingName.name()),
                                            QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                int qty = QInputDialog::getInt(this, "Количество",
                                               QString("Сколько '%1'?").arg(existingName.name()), 1, 1, 999, 1, &ok);
                if (ok) {
                    existingName.setId(-1);
                    existingName.setQuantity(qty);
                    existingName.setManufactureDate(QDate::currentDate());
                    StorageManager::instance().addToFridge(0, existingName);
                    showStatusMessage("✅ '" + existingName.name() + "' добавлен!");
                }
            }
            return;
        }

        // Всё ок — добавляем новый продукт и в каталог, и в холодильник
        StorageManager::instance().addToCatalog(p, 0);
        StorageManager::instance().addToFridge(0, p);
        showStatusMessage("✅ '" + p.name() + "' создан и добавлен!");
    }
    return;
    } else {
        QString bc = QInputDialog::getText(this, "Штрих-код", "Введите штрих-код:", QLineEdit::Normal, "", &ok);
        if (!ok || bc.isEmpty()) return;
        auto catalog = StorageManager::instance().getCatalogProducts("", 0);
        Product found;
        for (const auto &p : catalog) if (p.barcode() == bc) { found = p; break; }
        if (found.id() < 0) {
            int ret = QMessageBox::question(this, "Не найдено",
                                            "Штрих-код не найден в каталоге.\n\nХотите создать новый продукт с этим штрих-кодом?",
                                            QMessageBox::Yes | QMessageBox::No);
            if (ret == QMessageBox::Yes) {
                AddProductDialog dialog(this);
                dialog.setBarcode(bc); // ← Предзаполняем штрих-код
                if (dialog.exec() == QDialog::Accepted) {
                    Product p = dialog.getProduct();
                    // Проверки как в "Создать новый"
                    auto catalog = StorageManager::instance().getCatalogProducts("", 0);
                    bool exists = false;
                    for (const auto &cp : catalog) {
                        if (cp.barcode() == p.barcode() && !p.barcode().isEmpty()) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        StorageManager::instance().addToCatalog(p, 0);;
                        StorageManager::instance().addToFridge(0, p);
                        showStatusMessage("✅ '" + p.name() + "' создан и добавлен!");
                    }
                }
            }
            return;
        }
        Product exist = StorageManager::instance().getFridgeProductByBarcode(0, bc);
        if (exist.id() > 0) {
            int addQty = QInputDialog::getInt(this, "Уже есть",
                                              QString("'%1' уже есть. Сколько добавить?").arg(found.name()), 1, 1, 999, 1, &ok);
            if (ok) { exist.setQuantity(exist.quantity() + addQty); StorageManager::instance().updateFridgeProduct(0, exist); }
            return;
        }
        int qty = QInputDialog::getInt(this, "Количество", QString("Сколько '%1'?").arg(found.name()), 1, 1, 999, 1, &ok);
        if (!ok) return;

        bool dateOk = false;
        QString dateStr = QInputDialog::getText(this, "📅 Дата изготовления",
                                                QString("Введите дату изготовления '%1'\nФормат: ГГГГ-ММ-ДД").arg(found.name()),
                                                QLineEdit::Normal, QDate::currentDate().toString("yyyy-MM-dd"), &dateOk);

        if (!dateOk || dateStr.isEmpty()) return;

        QDate manDate = QDate::fromString(dateStr.trimmed(), "yyyy-MM-dd");
        if (!manDate.isValid()) {
            QMessageBox::warning(this, "Ошибка", "Неверный формат даты!");
            return;
        }

        Product np = found; np.setId(-1); np.setQuantity(qty);
        np.setManufactureDate(manDate);
        StorageManager::instance().addToFridge(0, np);
        showStatusMessage("✅ '" + np.name() + "' добавлен!");
    }
}

// ==================== ВЗЯТЬ ====================
void MainWindow::on_btnTakeProduct_clicked()
{
    int row = ui->tableProducts->currentRow();
    if (row < 0) { QMessageBox::information(this, "Выберите", "Выберите продукт."); return; }
    int id = ui->tableProducts->item(row, 0)->text().toInt();
    Product p = StorageManager::instance().getFridgeProduct(0, id);
    bool ok = false;
    int take = QInputDialog::getInt(this, "📤 Взять",
                                    QString("Сколько шт '%1'?\nВсего: %2 шт\n(0 = всё)").arg(p.name()).arg(p.quantity()),
                                    0, 0, p.quantity(), 1, &ok);
    if (!ok) return;

    if (take <= 0 || take >= p.quantity()) {
        int ret = QMessageBox::question(this, "Всё?", "Забрать всё?", QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            m_lastTakenProduct = p;
            m_lastTakenQuantity = p.quantity();
            m_hasLastTaken = true;

            StorageManager::instance().recordConsumption(0, p, p.quantity());
            StorageManager::instance().removeFromFridge(0, id);

            showStatusMessage("📤 '" + p.name() + "' изъят.");
        }
    } else {
        m_lastTakenProduct = p;
        m_lastTakenQuantity = take;
        m_hasLastTaken = true;
        p.setQuantity(p.quantity() - take);

        StorageManager::instance().recordConsumption(0, p, take);
        StorageManager::instance().updateFridgeProduct(0, p);

        showStatusMessage(QString("📤 Взято %1 шт '%2'. Осталось: %3 шт").arg(take).arg(p.name()).arg(p.quantity()));
    }
}

// ==================== ВЕРНУТЬ ====================
void MainWindow::on_btnReturnProduct_clicked()
{
    if (!m_hasLastTaken) { QMessageBox::information(this, "Нечего", "Нет последнего взятого."); return; }
    int ret = QMessageBox::question(this, "📥 Вернуть?",
                                    QString("Вернуть '%1' (%2 шт)?").arg(m_lastTakenProduct.name()).arg(m_lastTakenQuantity),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    Product exist = StorageManager::instance().getFridgeProductByBarcode(0, m_lastTakenProduct.barcode());
    if (exist.id() > 0) {
        exist.setQuantity(exist.quantity() + m_lastTakenQuantity);
        StorageManager::instance().updateFridgeProduct(0, exist);
    } else {
        Product rp = m_lastTakenProduct; rp.setId(-1); rp.setQuantity(m_lastTakenQuantity);
        StorageManager::instance().addToFridge(0, rp);
    }

    // Удаляем запись из истории
    StorageManager::instance().removeLastConsumption(0, m_lastTakenProduct.barcode(), m_lastTakenQuantity);

    showStatusMessage(QString("📥 Возвращено %1 шт '%2'.").arg(m_lastTakenQuantity).arg(m_lastTakenProduct.name()));
    m_hasLastTaken = false; m_lastTakenQuantity = 0;
}

void MainWindow::on_btnDeleteProduct_clicked()
{
    int row = ui->tableProducts->currentRow();
    if (row < 0) { QMessageBox::information(this, "Выберите", "Выберите продукт."); return; }
    int id = ui->tableProducts->item(row, 0)->text().toInt();
    Product p = StorageManager::instance().getFridgeProduct(0, id);
    int ret = QMessageBox::warning(this, "Удалить?", "Удалить '" + p.name() + "'?", QMessageBox::Yes | QMessageBox::No);
    if (ret == QMessageBox::Yes) { StorageManager::instance().removeFromFridge(0, id); showStatusMessage("🗑 Удалён."); }
}

void MainWindow::on_btnToggleFavorite_clicked()
{
    int row = ui->tableProducts->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Выберите продукт",
                                 "Пожалуйста, выберите продукт из таблицы.");
        return;
    }

    int id = ui->tableProducts->item(row, 0)->text().toInt();
    Product p = StorageManager::instance().getFridgeProduct(0, id);

    if (p.id() < 0) return;

    // Переключаем статус избранного
    p.setIsFavorite(!p.isFavorite());
    StorageManager::instance().updateFridgeProduct(0, p);

    if (p.isFavorite()) {
        showStatusMessage("⭐ '" + p.name() + "' добавлен в избранное!");
    } else {
        showStatusMessage("☆ '" + p.name() + "' убран из избранного.");
    }
}

void MainWindow::on_btnShoppingList_clicked() { ShoppingListDialog d(this); d.exec(); }
void MainWindow::on_btnHistory_clicked() { HistoryChart d(this); d.exec(); }
void MainWindow::on_lineEditSearch_textChanged(const QString &) { applyFilter(); }
void MainWindow::on_lineEditSearch_returnPressed() { applyFilter(); }
void MainWindow::on_comboBoxCategory_currentIndexChanged() { applyFilter(); }
void MainWindow::on_actionRefresh_triggered() { applyFilter(); showStatusMessage("🔄 Обновлено."); }
void MainWindow::on_actionShowFavorites_triggered() {
    m_showFavoritesOnly = !m_showFavoritesOnly; applyFilter();
    showStatusMessage(m_showFavoritesOnly ? "⭐ Избранное." : "📋 Все.");
}
void MainWindow::on_actionClearHistory_triggered() {
    QSqlDatabase db = QSqlDatabase::database(); QSqlQuery q(db);
    q.exec("DELETE FROM consumption_history"); showStatusMessage("🗑 История очищена.");
}
void MainWindow::on_actionExportReport_triggered()
{
    QString fn = QFileDialog::getSaveFileName(this, "Экспорт", "report.csv", "CSV (*.csv)");
    if (fn.isEmpty()) return;
    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream s(&f);
    s << "ID;Название;Штрих-код;Категория;Срок;Дней;Штук;Вес уп;Ед;Хранение\n";
    for (const auto &p : StorageManager::instance().getFridgeProducts(0))
        s << p.id() << ";" << p.name() << ";" << p.barcode() << ";" << p.category() << ";"
          << p.expirationDate().toString("dd.MM.yyyy") << ";" << p.daysUntilExpired() << ";"
          << p.quantity() << ";" << p.packageWeight() << ";" << p.weightUnit() << ";"
          << p.storageType() << "\n";
    f.close(); showStatusMessage("📄 Отчёт: " + fn);
    QDesktopServices::openUrl(QUrl::fromLocalFile(fn));
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "О программе",
                       "<h2>🧊 АРМ Специалиста — Умный Холодильник v2.0</h2>"
                       "<p><b>Назначение:</b> Автоматизация учёта продуктов в холодильных камерах "
                       "(бытовых и производственных).</p>"
                       "<hr>"
                       "<h3>📋 Основные функции:</h3>"
                       "<ul>"
                       "<li><b>📦 Добавление продуктов</b> — из каталога (с автодополнением) или по штрих-коду</li>"
                       "<li><b>📤 Взятие продуктов</b> — частичное (штучно) или полное изъятие из холодильника</li>"
                       "<li><b>📥 Возврат продуктов</b> — возврат последнего взятого продукта</li>"
                       "<li><b>⭐ Избранное</b> — добавление/удаление продуктов в избранное, фильтр по избранному</li>"
                       "<li><b>🔍 Поиск</b> — поиск по названию, штрих-коду или категории с автодополнением</li>"
                       "<li><b>📅 Отслеживание сроков годности</b> — автоматический расчёт срока от даты изготовления</li>"
                       "<li><b>⚠️ Уведомления</b> — о просроченных и заканчивающихся продуктах при запуске</li>"
                       "<li><b>📊 История потребления</b> — график использования продуктов за 7 дней</li>"
                       "<li><b>🛒 Список покупок</b> — автоматическое формирование на основе остатков</li>"
                       "<li><b>🧊 Визуализация</b> — графическое отображение заполнения холодильника</li>"
                       "<li><b>📄 Экспорт отчёта</b> — сохранение данных в CSV файл</li>"
                       "<li><b>🤖 Telegram-бот</b> — удалённое управление холодильником через смартфон</li>"
                       "</ul>"
                       "<hr>"
                       "<h3>🤖 Функции Telegram-бота:</h3>"
                       "<ul>"
                       "<li>📷 Сканирование штрих-кодов</li>"
                       "<li>🔍 Поиск продуктов по названию</li>"
                       "<li>📂 Выбор продуктов по категориям</li>"
                       "<li>🗑 Удаление продуктов</li>"
                       "<li>📊 Статистика и просмотр содержимого</li>"
                       "</ul>"
                       "<hr>"
                       "<p><b>💻 Стек технологий:</b> C++17, Qt 6, SQLite, QCharts, Telegram Bot API</p>"
                       "<p><b>👨‍💻 Разработчики:</b> Творческий проект группы РИС-25-3Б</p>"
                       "<p>© 2026</p>"
                       );
}

void MainWindow::startScannerServer()
{
    m_scannerServer = new QTcpServer(this);

    connect(m_scannerServer, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket *socket = m_scannerServer->nextPendingConnection();

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray data = socket->readAll();

            // Парсим HTTP запрос
            QString request = QString::fromUtf8(data);

            // Ищем JSON тело
            int jsonStart = request.indexOf("{");
            if (jsonStart < 0) {
                socket->write("HTTP/1.1 400 Bad Request\r\n\r\n");
                socket->close();
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(request.mid(jsonStart).toUtf8());
            QJsonObject obj = doc.object();
            QString barcode = obj["barcode"].toString();

            if (!barcode.isEmpty()) {
                // Ищем продукт в каталоге
                auto catalog = StorageManager::instance().getCatalogProducts("", 0);
                Product found;
                for (const auto &p : catalog) {
                    if (p.barcode() == barcode) {
                        found = p;
                        break;
                    }
                }

                if (found.id() > 0) {
                    // Продукт найден
                    showStatusMessage("📷 Отсканирован: " + found.name());

                    // Добавляем 1 шт сразу (без диалога, чтобы не блокировать)
                    QString scannedBarcode = barcode;
                    QTimer::singleShot(100, this, [this, scannedBarcode]() {
                        auto cat = StorageManager::instance().getCatalogProducts("", 0);
                        Product fnd;
                        for (const auto &p : cat) if (p.barcode() == scannedBarcode) { fnd = p; break; }

                        if (fnd.id() > 0) {
                            bool ok = false;
                            int qty = QInputDialog::getInt(this, "📷 Сканер",
                                                           QString("Отсканирован: %1\nСколько добавить в холодильник?").arg(fnd.name()),
                                                           1, 1, 999, 1, &ok);
                            if (ok) {
                                Product exist = StorageManager::instance().getFridgeProductByBarcode(0, scannedBarcode);
                                if (exist.id() > 0) {
                                    exist.setQuantity(exist.quantity() + qty);
                                    StorageManager::instance().updateFridgeProduct(0, exist);
                                } else {
                                    Product np = fnd;
                                    np.setId(-1);
                                    np.setQuantity(qty);
                                    np.setManufactureDate(QDate::currentDate());
                                    StorageManager::instance().addToFridge(0, np);
                                }
                                showStatusMessage("✅ '" + fnd.name() + "' добавлен (" + QString::number(qty) + " шт)");
                            }
                        }
                    });
                    socket->write("HTTP/1.1 200 OK\r\n\r\n{\"status\":\"ok\",\"name\":\"" + found.name().toUtf8() + "\"}");
                } else {
                    showStatusMessage("❌ Штрих-код не найден: " + barcode);
                    socket->write("HTTP/1.1 404 Not Found\r\n\r\n{\"status\":\"not_found\"}");

                    // Предлагаем создать новый продукт
                    QString scannedBc = barcode;
                    QTimer::singleShot(200, this, [this, scannedBc]() {
                        int ret = QMessageBox::question(this, "Штрих-код не найден",
                                                        "Штрих-код '" + scannedBc + "' не найден в каталоге.\n\nСоздать новый продукт?",
                                                        QMessageBox::Yes | QMessageBox::No);
                        if (ret == QMessageBox::Yes) {
                            AddProductDialog dialog(this);
                            dialog.setBarcode(scannedBc); // ← Предзаполняем штрих-код
                            if (dialog.exec() == QDialog::Accepted) {
                                Product p = dialog.getProduct();

                                auto catalog = StorageManager::instance().getCatalogProducts("", 0);
                                bool exists = false;
                                for (const auto &cp : catalog) {
                                    if (cp.barcode() == p.barcode()) { exists = true; break; }
                                }
                                if (!exists) {
                                    StorageManager::instance().addToCatalog(p, 0);;
                                }
                                StorageManager::instance().addToFridge(0, p);
                                showStatusMessage("✅ '" + p.name() + "' создан и добавлен!");
                            }
                        }
                    });
                }
            }
            socket->close();
        });
    });

    if (m_scannerServer->listen(QHostAddress::LocalHost, 8765)) {
        showStatusMessage("📷 Сервер сканера запущен на порту 8765");
    } else {
        showStatusMessage("❌ Ошибка запуска сервера сканера");
    }
}
