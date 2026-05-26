#include "addproductdialog.h"
#include "ui_addproductdialog.h"
#include <QMessageBox>
#include <QDate>
#include <QRandomGenerator>

AddProductDialog::AddProductDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AddProductDialog)
{
    ui->setupUi(this);
    ui->dateManufacture->setDate(QDate::currentDate());
}

AddProductDialog::~AddProductDialog() { delete ui; }

Product AddProductDialog::getProduct() const { return m_product; }

void AddProductDialog::on_btnSave_clicked()
{
    QString name = ui->lineEditName->text().trimmed();
    if (name.isEmpty()) { QMessageBox::warning(this, "Ошибка", "Введите название!"); return; }

    Product tempCheck; tempCheck.setName(name);
    if (!tempCheck.canBeStoredInFridge()) {
        int ret = QMessageBox::critical(this, "⚠️ Нельзя!", tempCheck.storageWarning(), QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::No) return;
    }

    m_product.setId(-1);
    m_product.setName(name);
    m_product.setBarcode(ui->lineEditBarcode->text().trimmed().isEmpty() ?
                             QString::number(QRandomGenerator::global()->bounded(100000000, 999999999)) :
                             ui->lineEditBarcode->text().trimmed());
    m_product.setManufactureDate(ui->dateManufacture->date());
    m_product.setShelfLifeDays(ui->spinShelfLife->value());
    m_product.setCategory(ui->comboCategory->currentText());
    m_product.setStorageType(ui->comboStorage->currentData().toString());
    m_product.setIsFavorite(ui->checkFavorite->isChecked());
    m_product.setQuantity(ui->spinQuantity->value());
    m_product.setPackageWeight(ui->spinWeight->value());
    m_product.setWeightUnit(ui->comboUnit->currentText());
    accept();
}

void AddProductDialog::setBarcode(const QString &barcode)
{
    ui->lineEditBarcode->setText(barcode);
}

void AddProductDialog::on_btnCancel_clicked() { reject(); }
