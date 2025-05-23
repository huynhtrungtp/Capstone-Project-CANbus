#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qcustomplot.h" // Thêm thư viện QCustomPlot
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>
#include "cansentworker.h"
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void writeCanData(const QByteArray &data); //

    QMap<QPair<int, int>, QString> nameMap;
    QMap<QPair<int, int>, QString> typeMap;
    QMap<QPair<int, int>, int> rowMap; // Lưu vị trí của (index, subindex)

private slots:


    void on_pushButton_clear_clicked();   // Xóa dữ liệu
    void readCanData();                    // Đọc dữ liệu từ Serial

    void on_pushButton_SPEED_clicked();
    void on_pushButton_ANGLE_clicked();
    void on_pushButton_clear_sent_clicked();
    void on_pushButton_okPort_clicked();
    void on_pushButton_refresh_clicked();
    void on_pushButton_sent_speed_clicked();
    void on_pushButton_send_pos_clicked();
    void on_pushButton_Mode_Speed_clicked();
    void on_pushButton_Mode_Position_clicked();

private:
    Ui::MainWindow *ui;
    QSerialPort *Serial;                 // Serial port object
    QTimer *timer;                         // Đọc dữ liệu theo chu kỳ
    void setupSerialPort();             // Cài đặt cổng COM
    void listAvailableSerialPorts();
    QLineEdit *lineEdit_value;
    void updateTextBrowser(const QString &data); // Hiển thị dữ liệu vào textBrowser
    void sendCanFrame();
    QByteArray hexStringToByteArray(const QString &hex);
    void updateTextBrowserSent(const QString &data);
    void parseCanData(const QByteArray &data);
    void updateTableValue1(int index, int subindex, const QString &dataHex, const QString &valueDecimal);
    void loadJsonData();
    void sendCANFrame(quint16 index, quint16 subindex, float value);
    void processReceivedCANFrame(const QByteArray &frame);
    void parseDataByPattern(const QByteArray &data);
    // Hàm tách header từ CAN data
    bool parseCanHeader(const QByteArray &data, int &index, int &subindex, QByteArray &valueData);
    // Chuyển dữ liệu sang decimal (int64)
    qint64 convertToDecimal(const QByteArray &valueData);
    // Chuyển dữ liệu sang float
    float convertToFloat(const QByteArray &valueData);
    // Xử lý các giá trị đặc biệt như angle, id, iq
    void handleSpecialValues(int index, int subindex, const QByteArray &valueData);
    QByteArray removeFrameCtrl(const QByteArray &rawData);


    // Dùng để tạo bảng input output
    void loadCANDefinitions();
    void appendToTableWidget(quint16 index, quint8 subindex, const QByteArray &valueData);
    QList<QJsonObject> canDefinitions;




    // Biến dùng cho đồ thị dòng điện
    QCustomPlot *customPlotCurrent;
    QTimer *currentTimer;

    // QVector<double> timeCurrentData, idData, iqData;
    // double elapsedCurrent = 0;

    void updateCurrentPlot();
    QVector<QPointF> currentBufferA, currentBufferB;
    QMutex currentBufferMutex;
    bool useCurrentBufferA = true;
    QVector<double> totalTimeCurrent, totalId, totalIq;
    double elapsedCurrent = 0;
    QElapsedTimer currentTimerElapsed;

    // biến dùng cho đồ thị điện áp
    QCustomPlot *customPlotVoltage;
    QMutex voltageBufferMutex;
    QVector<QPointF> voltageBufferA, voltageBufferB;
    QVector<double> totalTimeVoltage, totalUd, totalUq;
    bool useVoltageBufferA = true;
    QTimer *VoltageTimer;
    void updateVoltagePlot();
    double elapsedVoltage = 0;
    QElapsedTimer voltageTimerElapsed;

    // Biến dùng cho đồ thị theta
    QCustomPlot *customPlotTheta;
    QTimer *thetaPlotTimer;
    QElapsedTimer thetaTimer;

    QVector<QPointF> bufferA, bufferB;
    QVector<double> totalTimeTheta, totalThetaRef, totalThetaNow;
    QVector<double> thetaTimeData, thetaNowData, thetaRefData;
    QMutex bufferMutex;
    bool useBufferA = true;
    double elapsedTheta = 0;

    void updateThetaPlot();

    // dùng cho canworker(mutiple thread)
    QThread *canThread;
    CanWorker *canWorker;
signals:
    void sendCANCommand(quint16 index, quint16 subindex, double value);
};
#endif // MAINWINDOW_H

