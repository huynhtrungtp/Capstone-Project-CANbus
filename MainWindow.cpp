#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "qcustomplot.h" // Thư viện QCustomPlot
#include <QSerialPortInfo>
#include <QDebug>
#include <QMessageBox>
#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QMap>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <chrono>


// Biến tạm để lưu giá trị id/iq
float id_tmp = 0;
float iq_tmp = 0;
float theta_ref_global = 0.0f;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    ui->setupUi(this);


    loadJsonData(); // Load dữ liệu từ JSON khi khởi động
    // loadCANDefinitions();
    // Tạo đối tượng Serial
    Serial = new QSerialPort(this);
    // Kết nối tín hiệu với slot xử lý
    // ui->dothi->setTabText(0, "Graph Voltage");
    ui->dothi->setTabText(0, "Graph Current");
    ui->dothi->setTabText(1, "Graph Angle");
    ui->alltab->setTabText(0, "Graph and Controls");
    ui->alltab->setTabText(1, "PDO and SDO List");
    ui->alltab->setTabText(2, "Received and Sent Messages");



    // //////////////// đổi tên của sổ ////////////////////
    this->setWindowTitle("MOTOR_CONTROL_MONITOR");

    // Chế độ màu xen kẽ bảng PDO và SDO
    ui->tableWidget_2->setAlternatingRowColors(true);
    QPalette palette = ui->tableWidget_2->palette();
    palette.setColor(QPalette::Base, QColor("#E3F2FD"));
    palette.setColor(QPalette::AlternateBase, QColor("#F5F5F5"));
    ui->tableWidget_2->setPalette(palette);


    // Hiệu ứng khoảng cách và tự động dãn
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableWidget_2->setStyleSheet("QTableWidget::item { padding: 6px; }");

    // 1. Tạo thread và worker
    canThread = new QThread(this);
    canWorker = new CanWorker();
    canWorker->moveToThread(canThread);

    // 2. Truyền serial đang dùng
    canWorker->setSerial(Serial);

    // 3. Kết nối tín hiệu
    connect(canThread, &QThread::started, canWorker, &CanWorker::startListening);
    connect(this, &MainWindow::sendCANCommand, canWorker, &CanWorker::handleSendFrame);
    connect(canWorker, &CanWorker::logMessage, this, &MainWindow::updateTextBrowserSent);
    connect(ui->pushButton_refresh, &QPushButton::clicked, this, &MainWindow::listAvailableSerialPorts);
    connect(ui->pushButton_okPort, &QPushButton::clicked, this, &MainWindow::on_pushButton_okPort_clicked);
    connect(Serial, &QSerialPort::readyRead, this, &MainWindow::readCanData);


    // 4. Khởi động luồng
    canThread->start();

    listAvailableSerialPorts();


    // ////////////////////ĐỒ THỊ ĐIỆN ÁP //////////////////////////////////////
    // customPlotVoltage = new QCustomPlot(ui->customPlotWidget1);
    // customPlotVoltage->setGeometry(ui->customPlotWidget1->rect());
    // customPlotVoltage->setOpenGl(true); // Bật Double Buffer với OpenGL

    // // Graph 0: ud (xanh)
    // customPlotVoltage->addGraph();
    // customPlotVoltage->graph(0)->setPen(QPen(Qt::blue));
    // customPlotVoltage->graph(0)->setName("ud");

    // // Graph 1: uq (cam)
    // customPlotVoltage->addGraph();
    // customPlotVoltage->graph(1)->setPen(QPen(Qt::darkYellow));
    // customPlotVoltage->graph(1)->setName("uq");

    // // Cấu hình trục
    // customPlotVoltage->xAxis->setLabel("Time (s)");
    // customPlotVoltage->yAxis->setLabel("Voltage (V)");
    // customPlotVoltage->xAxis->setRange(0, 10);
    // customPlotVoltage->yAxis->setRange(-10, 10);  // Điều chỉnh tùy hệ thống
    // customPlotVoltage->legend->setVisible(true);

    // // Bật zoom và kéo bằng chuột trên cả hai trục
    // customPlotVoltage->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    // // customPlotVoltage->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);  // Zoom cả trục X và Y
    // //////////////////// ĐỒ THỊ DÒNG ĐIỆN //////////////////////////////////////////////
    customPlotCurrent = new QCustomPlot(ui->customPlotWidget2);
    customPlotCurrent->setGeometry(ui->customPlotWidget2->rect());
    customPlotCurrent->setOpenGl(true); // Bật Double Buffer với OpenGL

    // Graph 0: id (xanh)
    customPlotCurrent->addGraph();
    customPlotCurrent->graph(0)->setPen(QPen(Qt::blue));
    customPlotCurrent->graph(0)->setName("id");

    // Graph 1: iq (cam)
    customPlotCurrent->addGraph();
    customPlotCurrent->graph(1)->setPen(QPen(Qt::darkYellow));
    customPlotCurrent->graph(1)->setName("iq");

    // Cấu hình trục
    customPlotCurrent->xAxis->setLabel("Time (s)");
    customPlotCurrent->yAxis->setLabel("Current (A)");
    customPlotCurrent->xAxis->setRange(0, 10);
    customPlotCurrent->yAxis->setRange(-1, 1);  // Điều chỉnh tùy hệ thống
    customPlotCurrent->legend->setVisible(true);
    customPlotCurrent->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // //////////////////////////////// ĐỒ THỊ THETA ////////////////////////////////
    // Khởi tạo QCustomPlot cho góc theta (gắn vào customPlotWidget3)
    customPlotTheta = new QCustomPlot(ui->customPlotWidget3);
    customPlotTheta->setGeometry(ui->customPlotWidget3->rect());
    customPlotTheta->setOpenGl(true);

    // Thêm 2 đồ thị
    customPlotTheta->addGraph();  // theta_now
    customPlotTheta->graph(0)->setPen(QPen(Qt::darkGreen));
    customPlotTheta->graph(0)->setName("theta_now");

    customPlotTheta->addGraph();  // theta_ref
    customPlotTheta->graph(1)->setPen(QPen(Qt::red, 1, Qt::DashLine));
    customPlotTheta->graph(1)->setName("theta_ref");

    // Cấu hình trục
    customPlotTheta->xAxis->setLabel("Time (s)");
    customPlotTheta->yAxis->setLabel("Angle (rad)");
    customPlotTheta->xAxis->setRange(0, 10);
    customPlotTheta->yAxis->setRange(-10, 10);  // Điều chỉnh theo hệ thống của bạn
    customPlotTheta->legend->setVisible(true);
    customPlotTheta->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // //////////////// TIMER CẬP NHẬT ĐỒ THỊ THETA /////////////////////////////////
    thetaPlotTimer = new QTimer(this);
    connect(thetaPlotTimer, &QTimer::timeout, this, &MainWindow::updateThetaPlot);
    thetaPlotTimer->start(20);

    // //////////////// TIMER CẬP NHẬT ĐỒ THỊ DÒNG ĐIỆN /////////////////////////////////
    currentTimer = new QTimer(this);
    connect(currentTimer, &QTimer::timeout, this, &MainWindow::updateCurrentPlot);
    currentTimer->start(20); //
    // //////////////// TIMER CẬP NHẬT ĐỒ THỊ ĐIỆN ÁP /////////////////////////////////
    // VoltageTimer = new QTimer(this);
    // connect(VoltageTimer, &QTimer::timeout, this, &MainWindow::updateVoltagePlot);
    // VoltageTimer->start(20); //
}
// /////////////////////////////// THE TA //////////////////////////////////////
void MainWindow::updateThetaPlot()
{
    QVector<QPointF> plotData;

    bufferMutex.lock();
    useBufferA = !useBufferA;
    if (useBufferA) {
        plotData = bufferB;
        bufferB.clear();
    } else {
        plotData = bufferA;
        bufferA.clear();
    }
    bufferMutex.unlock();

    QVector<double> timeVector, thetaRefVec, thetaNowVec;

    for (const QPointF &point : plotData) {
        double timestamp = thetaTimer.elapsed() / 1000.0;
        timeVector.append(timestamp);
        thetaRefVec.append(point.x());
        thetaNowVec.append(point.y());
    }

    totalTimeTheta += timeVector;
    totalThetaRef += thetaRefVec;
    totalThetaNow += thetaNowVec;

    while (totalTimeTheta.size() > 400) {
        totalTimeTheta.removeFirst();
        totalThetaRef.removeFirst();
        totalThetaNow.removeFirst();
    }

    if (!totalTimeTheta.isEmpty()) {
        double lastTime = totalTimeTheta.last();
        customPlotTheta->xAxis->setRange(qMax(0.0, lastTime - 40), lastTime);
    }

    customPlotTheta->graph(0)->setData(totalTimeTheta, totalThetaNow);
    customPlotTheta->graph(1)->setData(totalTimeTheta, totalThetaRef);
    customPlotTheta->replot();
}

// /////////////////////////////// DÒNG ĐIỆN //////////////////////////////////////
// void MainWindow::updateCurrentPlot()
// {
//     QVector<QPointF> data;
//     currentBufferMutex.lock();
//     useCurrentBufferA = !useCurrentBufferA;
//     if (useCurrentBufferA) {
//         data = currentBufferB;
//         currentBufferB.clear();
//     } else {
//         data = currentBufferA;
//         currentBufferA.clear();
//     }
//     currentBufferMutex.unlock();

//     QVector<double> idVec, iqVec, timeVec;
//     for (const QPointF &point : data) {
//         elapsedCurrent += 0.05;
//         idVec.append(point.x());
//         iqVec.append(point.y());
//         timeVec.append(elapsedCurrent);
//     }

//     totalId += idVec;
//     totalIq += iqVec;
//     totalTimeCurrent += timeVec;

//     while (totalTimeCurrent.size() > 200) {
//         totalTimeCurrent.removeFirst();
//         totalId.removeFirst();
//         totalIq.removeFirst();
//     }
// customPlotTheta->xAxis->setRange(qMax(0.0, elapsedTheta - 10), elapsedTheta);
//     customPlotCurrent->graph(0)->setData(totalTimeCurrent, totalId);
//     customPlotCurrent->graph(1)->setData(totalTimeCurrent, totalIq);
//     customPlotCurrent->xAxis->setRange(qMax(0.0, elapsedCurrent - 10), elapsedCurrent);
//     customPlotCurrent->replot();

//     // Xóa dữ liệu cũ để tránh quá tải bộ nhớ //giữ 1000 điểm gần nhất//
//     int maxPoints = 1000;
//     if (thetaTimeData.size() > maxPoints) {
//         thetaTimeData.remove(0, thetaTimeData.size() - maxPoints);
//         thetaNowData.remove(0, thetaNowData.size() - maxPoints);
//         thetaRefData.remove(0, thetaRefData.size() - maxPoints);
// }
// }
void MainWindow::updateCurrentPlot()
{
    QVector<QPointF> data;

    currentBufferMutex.lock();
    useCurrentBufferA = !useCurrentBufferA;
    if (useCurrentBufferA) {
        data = currentBufferB;
        currentBufferB.clear();
    } else {
        data = currentBufferA;
        currentBufferA.clear();
    }
    currentBufferMutex.unlock();

    QVector<double> idVec, iqVec, timeVec;

    for (const QPointF &point : data) {
        double timestamp = currentTimerElapsed.elapsed() / 1000.0;
        timeVec.append(timestamp);
        idVec.append(point.x());
        iqVec.append(point.y());
    }

    totalTimeCurrent += timeVec;
    totalId += idVec;
    totalIq += iqVec;

    while (totalTimeCurrent.size() > 400) {
        totalTimeCurrent.removeFirst();
        totalId.removeFirst();
        totalIq.removeFirst();
    }

    if (!totalTimeCurrent.isEmpty()) {
        double lastTime = totalTimeCurrent.last();
        customPlotCurrent->xAxis->setRange(qMax(0.0, lastTime - 20), lastTime);
    }

    customPlotCurrent->graph(0)->setData(totalTimeCurrent, totalId);
    customPlotCurrent->graph(1)->setData(totalTimeCurrent, totalIq);
    customPlotCurrent->replot();
}

void MainWindow::updateVoltagePlot(){
    // QVector<QPointF> data;

    // voltageBufferMutex.lock();
    // useVoltageBufferA = !useVoltageBufferA;
    // if (useVoltageBufferA) {
    //     data = voltageBufferB;
    //     voltageBufferB.clear();
    // } else {
    //     data = voltageBufferA;
    //     voltageBufferA.clear();
    // }
    // voltageBufferMutex.unlock();

    // QVector<double> udVec, uqVec, timeVec;

    // for (const QPointF &point : data) {
    //     double timestamp = voltageTimerElapsed.elapsed() / 1000.0;
    //     timeVec.append(timestamp);
    //     udVec.append(point.x());
    //     uqVec.append(point.y());
    // }

    // totalTimeVoltage += timeVec;
    // totalUd += udVec;
    // totalUq += uqVec;

    // while (totalTimeVoltage.size() > 400) {
    //     totalTimeVoltage.removeFirst();
    //     totalUd.removeFirst();
    //     totalUq.removeFirst();
    // }

    // if (!totalTimeVoltage.isEmpty()) {
    //     double lastTime = totalTimeVoltage.last();
    //     customPlotVoltage->xAxis->setRange(qMax(0.0, lastTime - 20), lastTime);
    // }

    // customPlotVoltage->graph(0)->setData(totalTimeVoltage, totalUd);
    // customPlotVoltage->graph(1)->setData(totalTimeVoltage, totalUq);
    // customPlotVoltage->replot();
}


void MainWindow::listAvailableSerialPorts()
{
    ui->comboBox_serialPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        ui->comboBox_serialPort->addItem(port.portName(), port.portName());
    }

    if (ui->comboBox_serialPort->count() == 0)
        ui->comboBox_serialPort->addItem("No COM ports found");
}

void MainWindow::on_pushButton_refresh_clicked()
{
    listAvailableSerialPorts();
}

void MainWindow::on_pushButton_okPort_clicked()
{
    // QString selectedPort = ui->comboBox_serialPort->currentData().toString();
    // if (selectedPort.isEmpty() || selectedPort.contains("No COM ports")) {
    //     QMessageBox::warning(this, "Invalid Port", "Please select a valid COM port.");
    //     return;
    // }

    // // Đọc Baudrate từ comboBox_Baudrate
    // QString selectedBaud = ui->comboBox_Baudrate->currentText();

    // Serial->setPortName(selectedPort);
    // // Serial->setBaudRate(QSerialPort::Baud115200);
    // Serial->setBaudRate(selectedBaud.toInt());  // Thiết lập Baudrate
    // Serial->setDataBits(QSerialPort::Data8);
    // Serial->setParity(QSerialPort::NoParity);
    // Serial->setStopBits(QSerialPort::OneStop);
    // Serial->setFlowControl(QSerialPort::NoFlowControl);

    // if (Serial->open(QIODevice::ReadWrite)) {
    //     qDebug() << "Serial port opened: " << selectedPort;
    //     // connect(Serial, &QSerialPort::readyRead, this, &MainWindow::readCanData);
    //     QMessageBox::information(this, "Success", "Connected to " + selectedPort);
    //     ui->label_status->setText("Connected to " + selectedPort);
    // }
    // thetaTimer.start();
    // currentTimerElapsed.start();
    // voltageTimerElapsed.start();
    QString selectedPort = ui->comboBox_serialPort->currentData().toString();
    if (selectedPort.isEmpty() || selectedPort.contains("No COM ports")) {
        QMessageBox::warning(this, "Invalid Port", "Please select a valid COM port.");
        return;
    }

    // Đọc Baudrate từ comboBox_Baudrate
    QString selectedBaud = ui->comboBox_Baudrate->currentText();

    // Đóng cổng COM nếu đang mở
    if (Serial->isOpen()) {
        Serial->close();
        qDebug() << "Serial port closed before reconnecting.";
    }

    Serial->setPortName(selectedPort);
    Serial->setBaudRate(selectedBaud.toInt());
    Serial->setDataBits(QSerialPort::Data8);
    Serial->setParity(QSerialPort::NoParity);
    Serial->setStopBits(QSerialPort::OneStop);
    Serial->setFlowControl(QSerialPort::NoFlowControl);

    if (Serial->open(QIODevice::ReadWrite)) {
        qDebug() << "Serial port opened: " << selectedPort;
        // QMessageBox::information(this, "Success", "Connected to " + selectedPort);
        ui->label_status->setText("Connected to " + selectedPort);

        // Bắt đầu timer sau khi kết nối thành công
        thetaTimer.start();
        currentTimerElapsed.start();
        voltageTimerElapsed.start();
    } else {
        QMessageBox::critical(this, "Error", "Failed to open " + selectedPort);
        ui->label_status->setText("Disconnected");
    }
}


void MainWindow::readCanData()
{
    QByteArray data = Serial->readAll(); // Đọc dữ liệu từ Serial
    QString receivedData = data.toHex(' ').toUpper(); // Chuyển thành chuỗi HEX

    qDebug() << "Received Data: " << receivedData;

    // Tách dữ liệu từ byte đầu AA AA và byte cuối 55 55
    parseDataByPattern(data);
}

void MainWindow::parseDataByPattern(const QByteArray &data)
{
    QByteArray startPattern = QByteArray::fromHex("AA AA"); // Dấu hiệu bắt đầu
    QByteArray endPattern = QByteArray::fromHex("55 55");   // Dấu hiệu kết thúc

    int startIndex = 0;
    int endIndex = 0;

    // Tìm tất cả các phần dữ liệu bắt đầu với AA AA và kết thúc với 55 55
    while ((startIndex = data.indexOf(startPattern, startIndex)) != -1) {
        endIndex = data.indexOf(endPattern, startIndex);
        if (endIndex == -1) {
            qWarning() << "Không tìm thấy dấu kết thúc (55 55)!";
            break;
        }

        // Tách phần dữ liệu từ startIndex đến endIndex (bao gồm cả 55 55)
        QByteArray part = data.mid(startIndex, endIndex + endPattern.size() - startIndex);

        // Chuyển phần dữ liệu đã tách thành chuỗi HEX
        QString partHex = part.toHex(' ').toUpper();

        // Hiển thị phần dữ liệu đã tách vào textBrowser
        updateTextBrowser(partHex);

        // Gọi hàm parseCanData để xử lý dữ liệu đã tách
        parseCanData(part);

        // Tiếp tục tìm kiếm phần tiếp theo
        startIndex = endIndex + endPattern.size();
    }
}

void MainWindow::updateTextBrowser(const QString &data)
{
    // Lấy văn bản hiện tại từ textBrowser
    QString displayText = ui->textBrowser->toPlainText();

    // Lấy thời gian hiện tại với độ phân giải micro giây
    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    auto duration = now_us.time_since_epoch();
    qint64 total_us = duration.count();

    // Giờ hiện tại
    QTime time = QTime::currentTime();
    QString timeStr = time.toString("hh:mm:ss");

    // Lấy phần micro giây (sau mili giây)
    int micro = total_us % 1000000;  // micro giây (6 chữ số)
    QString microStr = QString(".%1").arg(micro, 6, 10, QChar('0'));

    // Gộp chuỗi thời gian hoàn chỉnh
    QString fullTime = timeStr + microStr;

    // Thêm dữ liệu và thời gian vào text
    ui->textBrowser->append(data + "  |  " + fullTime);

    // Tự động cuộn xuống cuối
    QTextCursor cursor = ui->textBrowser->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textBrowser->setTextCursor(cursor);

}



void MainWindow::on_pushButton_clear_clicked()
{
    ui->textBrowser->clear(); // Xóa toàn bộ dữ liệu
    qDebug() << "TextBrowser cleared!";
}


void MainWindow::on_pushButton_clear_sent_clicked()
{
    ui->textBrowser_sent->clear(); // Xóa toàn bộ dữ liệu
    qDebug() << " cleared!";
}


void MainWindow::updateTextBrowserSent(const QString &data)
{

    QString displayText = ui->textBrowser_sent->toPlainText();

    auto now = std::chrono::system_clock::now();
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    QTime time = QTime::currentTime();
    QString timeStr = time.toString("hh:mm:ss") + QString(".%1").arg(now_us % 1000000, 6, 10, QChar('0'));

    QString line = data + "  |  " + timeStr;
    ui->textBrowser_sent->append(line);

    QTextCursor cursor = ui->textBrowser_sent->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->textBrowser_sent->setTextCursor(cursor);

}

QByteArray MainWindow::removeFrameCtrl(const QByteArray &rawData) {
    QByteArray cleaned;
    for (int i = 0; i < rawData.size(); ++i) {
        if ((uchar)rawData[i] == 0xA5) {
            // Nếu là FrameCtrl, bỏ qua nó, lấy byte kế tiếp nếu có
            if (i + 1 < rawData.size()) {
                cleaned.append(rawData[i + 1]);
                ++i; // Bỏ qua byte tiếp theo (vì đã xử lý)
            }
        } else {
            cleaned.append(rawData[i]);
        }
    }
    return cleaned;
}


bool MainWindow::parseCanHeader(const QByteArray &data, int &index, int &subindex, QByteArray &valueData) {
    // Giải mã FrameCtrl nếu có
    QByteArray cleaned = removeFrameCtrl(data);

    if (cleaned.size() < 20) return false;

    index = (uchar)cleaned[3] << 8 | (uchar)cleaned[2];
    subindex = (uchar)cleaned[5] << 8 | (uchar)cleaned[4];
    valueData = cleaned.mid(6, 8);  // bạn có thể thay 8 bằng Length thật từ byte 14 nếu cần
    return true;
}

qint64 MainWindow::convertToDecimal(const QByteArray &valueData) {
    qint64 valueDecimal = 0;
    for (int i = 0; i < 4; i++) {
        valueDecimal = (valueDecimal << 8) | (uchar)valueData[i];
    }
    return valueDecimal;
}

float MainWindow::convertToFloat(const QByteArray &valueData) {
    float value = 0;
    if (valueData.size() >= 4) {
        memcpy(&value, valueData.constData(), sizeof(float));
    }
    return value;
}

void MainWindow::handleSpecialValues(int index, int subindex, const QByteArray &valueData) {
    // static float prev_id = 0.0f;
    // static float prev_iq = 0.0f;
    // float valueFloat = convertToFloat(valueData);
    // if (valueFloat > -1500 && valueFloat < 1500 && (valueFloat < -0.0000001 || valueFloat > 0.0000001)) {
    //     if (index == 0x01A0 && subindex == 0x0000) {
    //         // Angle
    //         ui->label_angle->setText(QString::number(valueFloat/3.14159265359, 'f', 4));
    //         float theta_now = valueFloat/3.14159265359;
    //         float theta_ref = theta_ref_global;
    //         double timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    //         thetaTimeData.append(timestamp);
    //         thetaNowData.append(theta_now);
    //         thetaRefData.append(theta_ref);

    //         QMutexLocker locker(&bufferMutex);
    //         if (useBufferA)
    //             bufferA.append(QPointF(theta_ref_global, theta_now));
    //         else
    //             bufferB.append(QPointF(theta_ref_global, theta_now));
    //     }

    //     else if (index == 0x02A0 && subindex == 0x0000 && valueData.size() >= 8) {
    //         // id và iq
    //         float id = convertToFloat(valueData.mid(0, 4));
    //         float iq = convertToFloat(valueData.mid(4, 4));

    //         if (id > -100 && id < 100 && (id < -0.00001 || id > 0.00001)) {
    //             if (qAbs(id - prev_id) <= 20.0f) {  // chỉ nhận nếu chênh lệch không quá 20
    //                 ui->label_id->setText(QString::number(id, 'f', 4));
    //                 id_tmp = id;
    //                 prev_id = id; // lưu giá trị để so sánh lần sau
    //             }
    //         }

    //         if (iq > -100 && iq < 100 && (iq < -0.00001 || iq > 0.00001)) {
    //             if (qAbs(iq - prev_iq) <= 20.0f) {
    //                 ui->label_iq->setText(QString::number(iq, 'f', 4));
    //                 iq_tmp = iq;
    //                 prev_iq = iq;

    //                 QMutexLocker locker(&currentBufferMutex);
    //                 if (useCurrentBufferA)
    //                     currentBufferA.append(QPointF(id_tmp, iq_tmp));
    //                 else
    //                     currentBufferB.append(QPointF(id_tmp, iq_tmp));
    //             }
    //         }
    //     }

    //     else if (index == 0x03A0 && subindex == 0x0000 && valueData.size() >= 8) {
    //         // vd và vq
    //         float vd = convertToFloat(valueData.mid(0, 4));
    //         // qDebug() << "Giá trị vd sau khi chuyển đổi:" << vd;
    //         // qDebug() << "Dữ liệu valueData (hex):" << valueData.toHex(' ').toUpper();
    //         float vq = convertToFloat(valueData.mid(4, 4));

    //         if (vd > -50 && vd < 50 && (vd < -0.01 || vd > 0.01))
    //             ui->label_vd->setText(QString::number(vd, 'f', 4));

    //         if (vq > -100 && vq < 100 && (vq < -0.01 || vq > 0.01))
    //             ui->label_vq->setText(QString::number(vq, 'f', 4));

    //         if ((qAbs(vd) > 0.0001 && qAbs(vd) < 50) &&
    //             (qAbs(vq) > 0.0001 && qAbs(vq) < 50)) {

    //             QMutexLocker locker(&voltageBufferMutex);
    //             if (useVoltageBufferA)
    //                 voltageBufferA.append(QPointF(vd, vq));
    //             else
    //                 voltageBufferB.append(QPointF(vd, vq));
    //         }
    //     }
    // }
    float valueFloat = convertToFloat(valueData);
    if (index == 0x01A0 && subindex == 0x0000) {
        // Angle
        ui->label_angle->setText(QString::number(valueFloat / 3.14159265359, 'f', 4));
        float theta_now = valueFloat / 3.14159265359;
        float theta_ref = theta_ref_global;
        double timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
        thetaTimeData.append(timestamp);
        thetaNowData.append(theta_now);
        thetaRefData.append(theta_ref);

        float speed_now = convertToFloat(valueData.mid(4, 4));
        ui->label_speed->setText(QString::number(speed_now, 'f', 4));

        QMutexLocker locker(&bufferMutex);
        if (useBufferA)
            bufferA.append(QPointF(theta_ref, theta_now));
        else
            bufferB.append(QPointF(theta_ref, theta_now));
    }

    else if (index == 0x02A0 && subindex == 0x0000 && valueData.size() >= 8) {
        // id và iq
        float id = convertToFloat(valueData.mid(0, 4));
        float iq = convertToFloat(valueData.mid(4, 4));

        ui->label_id->setText(QString::number(id, 'f', 4));
        ui->label_iq->setText(QString::number(iq, 'f', 4));
        id_tmp = id;
        iq_tmp = iq;

        QMutexLocker locker(&currentBufferMutex);
        if (useCurrentBufferA)
            currentBufferA.append(QPointF(id_tmp, iq_tmp));
        else
            currentBufferB.append(QPointF(id_tmp, iq_tmp));
    }

    // else if (index == 0x03A0 && subindex == 0x0000 && valueData.size() >= 8) {
    //     // vd và vq
    //     float vd = convertToFloat(valueData.mid(0, 4));
    //     float vq = convertToFloat(valueData.mid(4, 4));

    //     ui->label_vd->setText(QString::number(vd, 'f', 4));
    //     ui->label_vq->setText(QString::number(vq, 'f', 4));

    //     QMutexLocker locker(&voltageBufferMutex);
    //     if (useVoltageBufferA)
    //         voltageBufferA.append(QPointF(vd, vq));
    //     else
    //         voltageBufferB.append(QPointF(vd, vq));
    // }
    if (index == 0x03A0 && subindex == 0x0000 && valueData.size() >= 6) {
        // Đọc raw 2 byte theo little endian
        quint16 kp_raw = static_cast<quint16>(static_cast<quint8>(valueData[0]) | (static_cast<quint8>(valueData[1]) << 8));
        quint16 ki_raw = static_cast<quint16>(static_cast<quint8>(valueData[2]) | (static_cast<quint8>(valueData[3]) << 8));
        quint16 kd_raw = static_cast<quint16>(static_cast<quint8>(valueData[4]) | (static_cast<quint8>(valueData[5]) << 8));




        // Chuyển về float với scale (ví dụ: x100 → chia 100.0f)
        float kp = kp_raw/ 100.0f;
        float ki = ki_raw/ 100.0f;
        float kd = kd_raw/ 100.0f;

        // Hiển thị lên giao diện
        ui->kp_speed->setText(QString::number(kp, 'f', 2));
        ui->ki_speed->setText(QString::number(ki, 'f', 2));
        ui->kd_speed->setText(QString::number(kd, 'f', 2));


    }

    if (index == 0x04A0 && subindex == 0x0000 && valueData.size() >= 6) {
        // Đọc raw 2 byte theo Little Endian
        quint16 kp_raw = static_cast<quint8>(valueData[0]) | (static_cast<quint8>(valueData[1]) << 8);
        quint16 ki_raw = static_cast<quint8>(valueData[2]) | (static_cast<quint8>(valueData[3]) << 8);
        quint16 kd_raw = static_cast<quint8>(valueData[4]) | (static_cast<quint8>(valueData[5]) << 8);

        // Chuyển về float với scale (ví dụ: x100 → chia 100.0f)
        float kp = kp_raw / 100.0f;
        float ki = ki_raw / 100.0f;
        float kd = kd_raw / 100.0f;

        // Hiển thị lên giao diện
        ui->kp_pos->setText(QString::number(kp, 'f', 2));
        ui->ki_pos->setText(QString::number(ki, 'f', 2));
        ui->kd_pos->setText(QString::number(kd, 'f', 2));
    }


}

void MainWindow::parseCanData(const QByteArray &data) {
    int index, subindex;
    QByteArray valueData;

    if (!parseCanHeader(data, index, subindex, valueData)) {
        qDebug() << "Dữ liệu nhận được không hợp lệ!";
        return;
    }

    QString dataHex = valueData.toHex(' ').toUpper();
    qint64 valueDecimal = convertToDecimal(valueData);

    updateTableValue1(index, subindex, dataHex, QString::number(valueDecimal));

    // Gọi hàm mới dùng QByteArray thay vì float
    appendToTableWidget(index, subindex, valueData);

    handleSpecialValues(index, subindex, valueData);
    ui->tableWidget_2->resizeColumnsToContents();
}



// QMap<QPair<int, int>, int> rowMap;

// Hàm cập nhật giá trị vào bảng `QTableWidget_2`
void MainWindow::updateTableValue1(int index, int subindex, const QString &dataHex, const QString &valueDecimal) {
    QPair<int, int> key = qMakePair(index, subindex);
    QString name = nameMap.contains(key) ? nameMap[key] : "Unknown";
    QString type = typeMap.contains(key) ? typeMap[key] : "Unknown";

    // Chuyển index và subindex thành chuỗi với 4 chữ số (thêm số 0 vào trước nếu cần)
    QString indexStr = QString::number(index, 16).toUpper().rightJustified(4, '0');
    QString subindexStr = QString::number(subindex, 16).toUpper().rightJustified(4, '0');
    // Kiểm tra nếu ID đã tồn tại trong bảng
    if (rowMap.contains(key)) {
        int row = rowMap[key];
        ui->tableWidget_2->setItem(row, 3, new QTableWidgetItem(type)); // Cập nhật Type
        ui->tableWidget_2->setItem(row, 4, new QTableWidgetItem(dataHex));   // Cập nhật cột "Data"
        ui->tableWidget_2->setItem(row, 5, new QTableWidgetItem(valueDecimal)); //Cập nhật cột "Value"
    } else {
        // Thêm dòng mới
        int newRow = ui->tableWidget_2->rowCount();
        ui->tableWidget_2->insertRow(newRow);

        // Gán giá trị vào từng cột
        ui->tableWidget_2->setItem(newRow, 0, new QTableWidgetItem(name));  // Name
        ui->tableWidget_2->setItem(newRow, 1, new QTableWidgetItem(indexStr)); // Index
        ui->tableWidget_2->setItem(newRow, 2, new QTableWidgetItem(subindexStr)); // Subindex
        ui->tableWidget_2->setItem(newRow, 3, new QTableWidgetItem(type));  // Type
        ui->tableWidget_2->setItem(newRow, 4, new QTableWidgetItem(dataHex));  // data
        ui->tableWidget_2->setItem(newRow, 5, new QTableWidgetItem(valueDecimal));   // Cột "Value"

        // Lưu vị trí vào map để cập nhật nhanh hơn
        rowMap[key] = newRow;
    }


}

// Hàm đọc file JSON và lưu vào QMap
void MainWindow::loadJsonData() {
    QFile file("data.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Không thể mở file JSON!";
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    QJsonArray dataArray = doc.array();

    for (const QJsonValue &value : dataArray) {
        QJsonObject obj = value.toObject();
        int index = obj["index"].toString().toInt(nullptr, 16);
        int subindex = obj["subindex"].toString().toInt(nullptr, 16);
        QString name = obj["name"].toString();
        QString type = obj["type"].toString();
        QString dataHex = obj["data"].toString(); // Lấy dữ liệu "data" (nếu có)
        QString valueDecimal = obj["value"].toString(); // Lấy giá trị "value" (nếu có)

        QPair<int, int> key = qMakePair(index, subindex);
        nameMap[key] = name;
        typeMap[key] = type;

        updateTableValue1(index, subindex, dataHex, valueDecimal);
    }

    QJsonArray array = doc.array();
    for (const QJsonValue &val : array) {
        if (val.isObject()) {
            canDefinitions.append(val.toObject());
        }
    }
}

// nút nhấn gửi speed
void MainWindow::on_pushButton_SPEED_clicked()
{
    bool ok;
    float value = ui->lineEdit_value->text().toFloat(&ok);
    if (!ok) {
        qDebug() << "Lỗi: Giá trị không hợp lệ!";
        return;
    }

    emit sendCANCommand(0x0184, 0x0000, value);

    // Chuyển float thành QByteArray (8 byte)
    QByteArray valueData(8, 0);
    memcpy(valueData.data(), &value, sizeof(float));

    appendToTableWidget(0x0184, 0x0000, valueData);

}

// nút nhấn gửi angle
void MainWindow::on_pushButton_ANGLE_clicked()
{
    bool ok;
    float value = ui->lineEdit_value->text().toFloat(&ok);
    if (!ok) {
        qDebug() << "Lỗi: Giá trị không hợp lệ!";
        return;
    }

    value *= 3.14159265359;
    qDebug().nospace() << "value = " << QString::number(value, 'f', 10);

    emit sendCANCommand(0x0180, 0x0000, value);

    // Chuyển float thành QByteArray (8 byte)
    QByteArray valueData(8, 0);
    memcpy(valueData.data(), &value, sizeof(float));

    appendToTableWidget(0x0180, 0x0000, valueData);

    theta_ref_global = value / 3.14159265359;
}

// nút nhấn gửi mode speed
void MainWindow::on_pushButton_Mode_Speed_clicked()
{
    float value = 1.0f;
    emit sendCANCommand(0x0183, 0x0000, value);  // Gửi float

    // Tạo 8 byte để hiển thị lên bảng
    QByteArray valueData(8, 0);
    memcpy(valueData.data(), &value, sizeof(float));
    //hiển thi lên bảng
    appendToTableWidget(0x0183, 0x0000, valueData);
}


// nút nhấn gửi mode angle
void MainWindow::on_pushButton_Mode_Position_clicked()
{
    float value = 0.0f;
    emit sendCANCommand(0x0183, 0x0000, value);  // Gửi float

    // Tạo 8 byte để hiển thị lên bảng
    QByteArray valueData(8, 0);
    memcpy(valueData.data(), &value, sizeof(float));
    //hiển thi lên bảng
    appendToTableWidget(0x0183, 0x0000, valueData);
}

// nút nhấn gửi pid speed
void MainWindow::on_pushButton_sent_speed_clicked() {
    float kp = ui->kp_speed->text().toFloat();
    float ki = ui->ki_speed->text().toFloat();
    float kd = ui->kd_speed->text().toFloat();

    // Gửi PID Speed với index = 0x03A0
    if (canWorker) {
        canWorker->sendPIDFrame(0x03A0, kp, ki, kd);
    }
}

// nút nhấn gửi pid pos
void MainWindow::on_pushButton_send_pos_clicked() {
    float kp = ui->kp_pos->text().toFloat();
    float ki = ui->ki_pos->text().toFloat();
    float kd = ui->kd_pos->text().toFloat();

    // Gửi PID Position với index = 0x04A0
    if (canWorker) {
        canWorker->sendPIDFrame(0x04A0, kp, ki, kd);
    }
}


// void MainWindow::loadCANDefinitions()
// {
//     QFile file("data1.json");
//     if (!file.open(QIODevice::ReadOnly)) {
//         qDebug() << "Không thể mở file JSON";
//         return;
//     }

//     QByteArray jsonData = file.readAll();
//     QJsonDocument doc = QJsonDocument::fromJson(jsonData);
//     if (!doc.isArray()) {
//         qDebug() << "Dữ liệu JSON không phải mảng!";
//         return;
//     }

//     QJsonArray array = doc.array();
//     for (const QJsonValue &val : array) {
//         if (val.isObject()) {
//             canDefinitions.append(val.toObject());
//         }
//     }
// }
// /////////// HÀM HIỂN THỊ DỮ LIỆU LÊN TABLETABWIGET input output //////////////////
// void MainWindow::appendToTableWidget(quint16 index, quint8 subindex, double data)
// {
//     QString name = "Unknown";
//     QString type = "Unknown";

//     // Tìm trong danh sách đã load từ JSON
//     for (const QJsonObject &obj : canDefinitions) {
//         bool ok;
//         if (obj["index"].toString().toUShort(&ok, 16) == index &&
//             obj["subindex"].toString().toUShort(&ok, 16) == subindex) {
//             name = obj["name"].toString();
//             type = obj["type"].toString();
//             break;
//         }
//     }

//     QString indexStr = QString("%1").arg(index, 4, 16, QLatin1Char('0')).toUpper();
//     QString subindexStr = QString("%1").arg(subindex, 4, 16, QLatin1Char('0')).toUpper();

//     // ❗ Chuyển `double data` sang float (4 byte) giống bảng 2
//     float floatData = static_cast<float>(data);
//     QByteArray byteArray(reinterpret_cast<const char*>(&floatData), sizeof(float));
//     QString hexData;
//     for (int i = 0; i < byteArray.size(); ++i) {
//         hexData += QString("%1 ").arg(static_cast<quint8>(byteArray[i]), 2, 16, QLatin1Char('0')).toUpper();
//     }
//     hexData = hexData.trimmed(); // Bỏ khoảng trắng cuối

//     // Kiểm tra nếu đã có dòng trùng index + subindex
//     int rowCount = ui->tableWidget->rowCount();
//     for (int i = 0; i < rowCount; ++i) {
//         QString existingIndex = ui->tableWidget->item(i, 1)->text();
//         QString existingSubindex = ui->tableWidget->item(i, 2)->text();
//         if (existingIndex == indexStr && existingSubindex == subindexStr) {
//             // Cập nhật dòng
//             ui->tableWidget->item(i, 0)->setText(name);
//             ui->tableWidget->item(i, 3)->setText(type);
//             ui->tableWidget->item(i, 4)->setText(hexData);

//             // Tô màu dòng theo index
//             QColor rowColor = QColor(255, 255, 255); // mặc định trắng
//             if (index >= 0x01A0 && index <= 0x04A0) {
//                 rowColor = QColor(255, 200, 200); // Output – đỏ nhạt
//             } else if (index >= 0x0180 && index <= 0x0190) {
//                 rowColor = QColor(200, 255, 200); // Input – xanh lá nhạt
//             }

//             for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
//                 ui->tableWidget->item(i, col)->setBackground(rowColor);
//             }

//             return;
//         }
//     }

//     // Nếu không trùng, thêm dòng mới
//     int row = ui->tableWidget->rowCount();
//     ui->tableWidget->insertRow(row);
//     ui->tableWidget->setItem(row, 0, new QTableWidgetItem(name));
//     ui->tableWidget->setItem(row, 1, new QTableWidgetItem(indexStr));
//     ui->tableWidget->setItem(row, 2, new QTableWidgetItem(subindexStr));
//     ui->tableWidget->setItem(row, 3, new QTableWidgetItem(type));
//     ui->tableWidget->setItem(row, 4, new QTableWidgetItem(hexData));

//     // Tô màu dòng mới theo index
//     QColor rowColor = QColor(255, 255, 255); // mặc định trắng
//     if ((index >= 0x01A0 && index <= 0x04A0) || (index == 0x0720)) {
//         rowColor = QColor(255, 200, 200); // Output – đỏ nhạt
//     } else if (index >= 0x0180 && index <= 0x0190) {
//         rowColor = QColor(200, 255, 200); // Input – xanh lá nhạt
//     }

//     for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
//         ui->tableWidget->item(row, col)->setBackground(rowColor);
//     }
// }
void MainWindow::appendToTableWidget(quint16 index, quint8 subindex, const QByteArray &valueData)
{
    QString name = "Unknown";
    QString type = "Unknown";

    // Tìm trong danh sách đã load từ JSON
    for (const QJsonObject &obj : canDefinitions) {
        bool ok;
        if (obj["index"].toString().toUShort(&ok, 16) == index &&
            obj["subindex"].toString().toUShort(&ok, 16) == subindex) {
            name = obj["name"].toString();
            type = obj["type"].toString();
            break;
        }
    }

    QString indexStr = QString("%1").arg(index, 4, 16, QLatin1Char('0')).toUpper();
    QString subindexStr = QString("%1").arg(subindex, 4, 16, QLatin1Char('0')).toUpper();

    // Chuyển valueData thành chuỗi hex
    QString hexData;
    for (int i = 0; i < valueData.size(); ++i) {
        hexData += QString("%1 ").arg(static_cast<quint8>(valueData[i]), 2, 16, QLatin1Char('0')).toUpper();
    }
    hexData = hexData.trimmed();

    // Kiểm tra nếu đã có dòng trùng index + subindex
    int rowCount = ui->tableWidget->rowCount();
    for (int i = 0; i < rowCount; ++i) {
        QString existingIndex = ui->tableWidget->item(i, 1)->text();
        QString existingSubindex = ui->tableWidget->item(i, 2)->text();
        if (existingIndex == indexStr && existingSubindex == subindexStr) {
            // Cập nhật dòng
            ui->tableWidget->item(i, 0)->setText(name);
            ui->tableWidget->item(i, 3)->setText(type);
            ui->tableWidget->item(i, 4)->setText(hexData);

            // Tô màu dòng theo index
            QColor rowColor = QColor(255, 255, 255); // mặc định trắng
            if ((index >= 0x01A0 && index <= 0x04A0) || (index == 0x0720)) {
                rowColor = QColor(255, 200, 200); // Output – đỏ nhạt
            } else if (index >= 0x0180 && index <= 0x0190) {
                rowColor = QColor(200, 255, 200); // Input – xanh lá nhạt
            }

            for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
                ui->tableWidget->item(i, col)->setBackground(rowColor);
            }

            return;
        }
    }

    // Nếu không trùng, thêm dòng mới
    int row = ui->tableWidget->rowCount();
    ui->tableWidget->insertRow(row);
    ui->tableWidget->setItem(row, 0, new QTableWidgetItem(name));
    ui->tableWidget->setItem(row, 1, new QTableWidgetItem(indexStr));
    ui->tableWidget->setItem(row, 2, new QTableWidgetItem(subindexStr));
    ui->tableWidget->setItem(row, 3, new QTableWidgetItem(type));
    ui->tableWidget->setItem(row, 4, new QTableWidgetItem(hexData));

    // Tô màu dòng mới theo index
    QColor rowColor = QColor(255, 255, 255); // mặc định trắng
    if ((index >= 0x01A0 && index <= 0x04A0) || (index == 0x0720)) {
        rowColor = QColor(255, 200, 200); // Output – đỏ nhạt
    } else if (index >= 0x0180 && index <= 0x0190) {
        rowColor = QColor(200, 255, 200); // Input – xanh lá nhạt
    }

    for (int col = 0; col < ui->tableWidget->columnCount(); ++col) {
        ui->tableWidget->item(row, col)->setBackground(rowColor);
    }
}



MainWindow::~MainWindow()
{
    canThread->quit();
    canThread->wait();
    delete canWorker;
    delete ui;
}




