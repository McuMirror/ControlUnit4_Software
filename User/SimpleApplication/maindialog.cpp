#include "maindialog.h"
#include "ui_maindialog.h"
#include <QElapsedTimer>
#include <QDebug>
#include <QSettings>
#include "Interfaces/curs485iointerface.h"
#include "../qCustomLib/qCustomLib.h"
#include "sspdwidgetm1.h"
#include "heaterwidget.h"

#ifdef RASPBERRY_PI
#include <QApplication>
#include <QDesktopWidget>
#endif

MainDialog::MainDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::MainDialog)
    , mInterface(new cInterface(this))
    , mTimer(new QTimer(this))
{

    ui->setupUi(this);
#ifdef RASPBERRY_PI
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::HLine);
    frame->setFrameShadow(QFrame::Plain);
    QLabel *title = new QLabel(QString("Control Unit application").toUpper());
    title->setAlignment(Qt::AlignHCenter);
    QFont font = title->font();
    font.setBold(true);
    title->setFont(font);

    QPushButton *pPB = new QPushButton ("x");
    pPB->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    pPB->setFlat(true);
    pPB->setFixedHeight(QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight));
    pPB->setFixedWidth(QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight));

    QVBoxLayout *vlayout = new QVBoxLayout(ui->wTitle);
    QHBoxLayout *hlayout = new QHBoxLayout(ui->wTitle);
    hlayout->addWidget(title);
    hlayout->addWidget(pPB);

    vlayout->addLayout(hlayout);
    vlayout->addWidget(frame);
    vlayout->setMargin(0);

    connect(pPB,SIGNAL(clicked()), this, SLOT(close()));

    this->setWindowState(Qt::WindowFullScreen);
#else
    ui->wTitle->hide();
#endif

    mTimer->setSingleShot(true);

}

MainDialog::~MainDialog()
{
    delete ui;
}

void MainDialog::on_stackedWidget_currentChanged(int arg1)
{
    if (!mInited) return;
    // закрываем старое окно
    if (lastWidgetIndex>-1){
        auto * tmpWidget = qobject_cast<CommonWidget *>(ui->stackedWidget->widget(lastWidgetIndex));
        if (tmpWidget)
            tmpWidget->closeWidget();
    }

    lastWidgetIndex = arg1;

    auto *tmpWidget = qobject_cast<CommonWidget *>(ui->stackedWidget->widget(arg1));
    if (tmpWidget)
        tmpWidget->openWidget();
}

void MainDialog::changeTimeOut(int msecs)
{
    mTimerTimeOut = msecs;
}

void MainDialog::timerTimeOut()
{
    auto *tmpWidget = qobject_cast<CommonWidget *>(ui->stackedWidget->currentWidget());
    if (tmpWidget)
        tmpWidget->updateWidget();

    mTimer->start(mTimerTimeOut);
}

void MainDialog::setAddress(const QHostAddress &address)
{
    mAddress = address;
}

bool MainDialog::initialize()
{
    // иницилизация порта и всего-всего.
    mElapsedTimer.start();
    qDebug()<<"Initialize";

    bool ok;
    QString answer;

    if (mTcpIpProtocol){
        auto* interface = new cuTcpSocketIOInterface(this);

        interface->setAddress(convertToHostAddress(mTcpIpAddress));
        interface->setPort(SERVER_TCPIP_PORT);

        answer = interface->tcpIpQuery("SYST:DEVL?\r\n", 500, &ok);
        mInterface = interface;
    }
    else {
        auto* interface = new cuRs485IOInterface(this);
        interface->setPortName(mSerialPortName);

        QSettings settings("Scontel", "RaspPi Server");
        int size = settings.beginReadArray("devices");

        answer.append(QString("DevCount: %1\r\n").arg(size));

        for (int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);

            answer.append(";<br>");
            answer.append(QString("Dev%1: address=%2: type=%3\r\n")
                          .arg(i)
                          .arg(settings.value("devAddress", 255).toInt())
                          .arg(settings.value("devType","None").toString()));
        }

        settings.endArray();
        mInterface = interface;
    }

    connect(mInterface, SIGNAL(msgReceived(quint8,quint8,quint8,quint8*)),
            this, SLOT(msgReceived(quint8,quint8,quint8,quint8*)));

    // получаем список доступных устройств.
    mWaiting = true;

    qDebug()<<"answer: "<<answer;
    createUI(answer);

    connect(mTimer, SIGNAL(timeout()), SLOT(timerTimeOut()));
    mTimer->start(mTimerTimeOut);
    mInited = true;
    on_stackedWidget_currentChanged(0);
    return true;
}

void MainDialog::msgReceived(quint8 address, quint8 command, quint8 dataLength, quint8* data)
{
    Q_UNUSED(address)
    mWaiting = false;
    switch (command) {
    case CMD_SERVER_GET_DEVICE_LIST:
        qDebug()<<"CMD_SERVER_GET_DEVICE_LIST";
        QString answer = QString(QByteArray(reinterpret_cast<char*>(data), dataLength));
        qDebug()<<"Answer: "<<answer;
        createUI(answer);
        break;
    }
}

void MainDialog::setSerialPortName(const QString &serialPortName)
{
    mSerialPortName = serialPortName;
}

void MainDialog::setTcpIpAddress(const QString &tcpIpAddress)
{
    mTcpIpAddress = tcpIpAddress;
}

void MainDialog::setTcpIpProtocolEnable(bool tcpIpProtocol)
{
    mTcpIpProtocol = tcpIpProtocol;
}


bool MainDialog::waitingAnswer()
{
    QElapsedTimer timer;
    timer.start();
    while (mWaiting && (timer.elapsed()<1000))
        qApp->processEvents();
    return !mWaiting;
}

bool MainDialog::createUI(const QString& deviceList)
{
    QStringList list = deviceList.split("<br>");
    // составляем список всех устройств, каждый из них инициализируем  и т. д.
    foreach (QString str, list) {
        QStringList lList = str.split(':');
        if (lList.size() == 3){ //должно быть описание устройства
            //первое значение DevX - X порядковый номер
            quint8 address = static_cast<quint8>(lList[1].split('=')[1].toInt());
            QString type = lList[2].split('=')[1];
            CommonDriver * tmpDriver = nullptr;
            if (type.contains("CU4SDM0")){
                //данное устройство - SspdDriver
                tmpDriver = new SspdDriverM0(this);
                ui->listWidget->addItem(QString("Sspd Driver\nAddress: %1").arg(address));
                auto* widget = new SspdWidget(this);
                widget->setDriver(qobject_cast<SspdDriverM0*>(tmpDriver));
                ui->stackedWidget->addWidget(widget);
            }
            else if (type.contains("CU4SDM1")){
                //данное устройство - SspdDriver
                tmpDriver = new SspdDriverM1(this);
                ui->listWidget->addItem(QString("Sspd Driver\nAddress: %1").arg(address));
                auto* widget = new SspdWidgetM1(this);
                widget->setDriver(qobject_cast<SspdDriverM1*>(tmpDriver));
                ui->stackedWidget->addWidget(widget);
            }
            else if (type.contains("CU4TDM0")){
                //данное устройство - TempDriver
                tmpDriver = new TempDriverM0(this);
                ui->listWidget->addItem(QString("Temperature\nAddress: %1").arg(address));
                auto* widget = new TempWidget(this);
                widget->setDriver(qobject_cast<TempDriverM0*>(tmpDriver));
                ui->stackedWidget->addWidget(widget);
            }
            else if (type.contains("CU4TDM1")){
                //данное устройство - TempDriver
                tmpDriver = new TempDriverM1(this);
                ui->listWidget->addItem(QString("Temperature (M1)\nAddress: %1").arg(address));

                auto* widget = new TempM1Widget(this);
                widget->setDriver(qobject_cast<TempDriverM1*>(tmpDriver));
                widget->setTempReset(new TemperatureRecycleInterface(qobject_cast<TempDriverM1*>(tmpDriver),&mDrivers,widget));
                ui->stackedWidget->addWidget(widget);
            }
            else if (type.contains("CU4HTM")){
                //данное устройство - HeaterDriver
                tmpDriver = new HeaterDriverM0(this);
                ui->listWidget->addItem(QString("Heater\nAddress: %1").arg(address));

                auto* widget = new HeaterWidget(this);
                widget->setDriver(qobject_cast<HeaterDriverM0*>(tmpDriver));
                ui->stackedWidget->addWidget(widget);
            }

            if (tmpDriver != nullptr){
                tmpDriver->setDevAddress(address);
                tmpDriver->setIOInterface(mInterface);
                mDrivers.append(tmpDriver);
            }
        }
    }

    ui->listWidget->addItem("All Channels");
    aWidget = new AllChannels(this);
    ui->stackedWidget->addWidget(aWidget);
    aWidget->initialize(mDrivers);
    aWidget->setInterface(mInterface);

    connect(aWidget, SIGNAL(setTimeOut(int)), SLOT(changeTimeOut(int)));
    return true;
}
