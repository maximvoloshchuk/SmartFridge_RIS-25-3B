QT       += core gui sql charts network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    product.cpp \
    storagemanager.cpp \
    notificationmanager.cpp \
    fridgewidget.cpp \
    addproductdialog.cpp \
    shoppinglistdialog.cpp \
    historychart.cpp \
    telegrambot.cpp

HEADERS += \
    mainwindow.h \
    product.h \
    storagemanager.h \
    notificationmanager.h \
    fridgewidget.h \
    addproductdialog.h \
    shoppinglistdialog.h \
    historychart.h \
    telegrambot.h

FORMS += \
    mainwindow.ui \
    addproductdialog.ui \
    shoppinglistdialog.ui \
    historychart.ui

DISTFILES += \
    decode_barcode.py \
    scanner.py \
    test_camera.py