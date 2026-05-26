#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QStringList>

class TelegramBot : public QObject
{
    Q_OBJECT

public:
    explicit TelegramBot(const QString &botToken, QObject *parent = nullptr);
    ~TelegramBot();

    void start();
    void stop();

signals:
    void productAdded(const QString &productName, double quantity, const QString &unit);
    void botStatusChanged(const QString &status);

private slots:
    void checkUpdates();

private:
    QString m_botToken;
    QString m_apiUrl;
    QNetworkAccessManager *m_networkManager;
    QTimer *m_pollTimer;
    int m_lastUpdateId;
    bool m_running;

    struct UserState {
        qint64 chatId;
        QString state;
        QString barcode;
        QString productName;
        QString productUnit;
        QStringList searchResults;
        // Для создания нового продукта
        QString newProductName;
        QString newProductCategory;
        int newProductShelfLife = 7;
        double newProductWeight = 1.0;
        QString newProductWeightUnit = "шт";
    };
    QMap<qint64, UserState> m_userStates;

    void processUpdate(const QJsonObject &update);
    void sendMessage(qint64 chatId, const QString &text, const QJsonArray &replyMarkup = QJsonArray());
    void processBarcode(qint64 chatId, const QString &barcode);
    void processQuantity(qint64 chatId, const QString &quantityText);

    void showUserFridge(qint64 chatId);
    void showUserExpiringSoon(qint64 chatId);
    void showUserExpired(qint64 chatId);
    void showUserStats(qint64 chatId);
    void showUserHistory(qint64 chatId);
    void startTakeProduct(qint64 chatId);
    void processPhoto(qint64 chatId, const QJsonObject &photo);

    QJsonArray mainKeyboard();
    QJsonArray cancelKeyboard();
};

#endif // TELEGRAMBOT_H
