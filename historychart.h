#ifndef HISTORYCHART_H
#define HISTORYCHART_H

#include <QDialog>

namespace Ui {
class HistoryChart;
}

class HistoryChart : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryChart(QWidget *parent = nullptr);
    ~HistoryChart();

private:
    Ui::HistoryChart *ui;
    void buildChart();
};

#endif // HISTORYCHART_H
