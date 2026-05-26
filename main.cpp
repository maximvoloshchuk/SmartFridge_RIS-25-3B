#include <QApplication>
#include "mainwindow.h"
#include "storagemanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Тёмная тема для всего приложения
    a.setStyleSheet(R"(
        QMainWindow {
            background-color: #1e1e2e;
        }
        QDialog {
            background-color: #1e1e2e;
        }
        QLabel {
            color: #cdd6f4;
            font-size: 13px;
        }
        QPushButton {
            background-color: #89b4fa;
            color: #1e1e2e;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: bold;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #74c7ec;
        }
        QPushButton:pressed {
            background-color: #89dceb;
        }
        QTableWidget {
            background-color: #313244;
            color: #cdd6f4;
            gridline-color: #45475a;
            border: 1px solid #45475a;
            border-radius: 6px;
            font-size: 12px;
        }
        QTableWidget::item {
            padding: 6px;
        }
        QTableWidget::item:selected {
            background-color: #89b4fa;
            color: #1e1e2e;
        }
        QHeaderView::section {
            background-color: #45475a;
            color: #cdd6f4;
            padding: 6px;
            border: none;
            font-weight: bold;
        }
        QLineEdit, QDateEdit, QComboBox, QSpinBox {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 4px;
            padding: 6px;
            font-size: 13px;
        }
        QLineEdit:focus, QDateEdit:focus, QComboBox:focus {
            border-color: #89b4fa;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #313244;
            color: #cdd6f4;
            selection-background-color: #89b4fa;
        }
        QCheckBox {
            color: #cdd6f4;
        }
        QStatusBar {
            background-color: #181825;
            color: #a6adc8;
            font-size: 12px;
        }
        QMenuBar {
            background-color: #181825;
            color: #cdd6f4;
        }
        QMenuBar::item:selected {
            background-color: #45475a;
        }
        QMenu {
            background-color: #313244;
            color: #cdd6f4;
        }
        QMenu::item:selected {
            background-color: #89b4fa;
            color: #1e1e2e;
        }
        QTextBrowser {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            border-radius: 6px;
        }
    )");

    // Инициализация базы данных
    if (!StorageManager::instance().initializeDatabase()) {
        return -1;
    }

    MainWindow w;
    w.show();

    return a.exec();
}
