#include "historychart.h"
#include "ui_historychart.h"
#include "storagemanager.h"
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QVBoxLayout>
#include <QMap>

HistoryChart::HistoryChart(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HistoryChart)
{
    ui->setupUi(this);
    buildChart();
}

HistoryChart::~HistoryChart()
{
    delete ui;
}

void HistoryChart::buildChart()
{
    auto history = StorageManager::instance().getConsumptionHistory(0, 7);
    QMap<QString, int> productCount;
    for (const auto &rec : history) {
        productCount[rec.productName] += rec.quantity;
    }

    if (productCount.isEmpty()) {
        ui->labelNoData->setText("Нет данных за последние 7 дней");
        ui->labelNoData->show();
        return;
    }
    ui->labelNoData->hide();

    QBarSet *set = new QBarSet("Взято раз");
    QStringList categories;

    for (auto it = productCount.begin(); it != productCount.end(); ++it) {
        *set << it.value();
        categories << it.key();
    }

    QBarSeries *series = new QBarSeries();
    series->append(set);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("📊 История потребления продуктов (7 дней)");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setTheme(QChart::ChartThemeDark);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    int maxVal = 0;
    for (const auto &val : productCount.values()) {
        if (val > maxVal) maxVal = val;
    }

    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, qMax(1, maxVal + 1));
    axisY->setTickCount(qMax(2, maxVal + 2));  // Целые деления
    axisY->setLabelFormat("%d");                // Формат целых чисел
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    QChartView *chartView = new QChartView(chart, ui->widgetChartContainer);
    chartView->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout *layout = new QVBoxLayout(ui->widgetChartContainer);
    layout->addWidget(chartView);
}
