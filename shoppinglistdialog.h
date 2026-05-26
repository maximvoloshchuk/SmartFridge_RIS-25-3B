#ifndef SHOPPINGLISTDIALOG_H
#define SHOPPINGLISTDIALOG_H

#include <QDialog>

namespace Ui {
class ShoppingListDialog;
}

class ShoppingListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShoppingListDialog(QWidget *parent = nullptr);
    ~ShoppingListDialog();

private:
    Ui::ShoppingListDialog *ui;
    void generateShoppingList();
};

#endif // SHOPPINGLISTDIALOG_H
