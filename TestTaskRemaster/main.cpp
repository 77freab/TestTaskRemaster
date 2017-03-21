#include "main.h"
#include <osg/MatrixTransform>

TestTaskRemaster::TestTaskRemaster(QWidget *parent)
  : QWidget(parent)
{
  // создание элементов интерфейса
  _lblA = new QLabel("A");
  _lblB = new QLabel("B");

  _sldrA = new QSlider(Qt::Horizontal);
  _sldrA->setTickPosition(QSlider::TicksBothSides);
  _sldrA->setRange(-5, 5);

  _sldrB = new QSlider(Qt::Horizontal);
  _sldrB->setTickPosition(QSlider::TicksBothSides);
  _sldrB->setRange(-5, 5);

  _spnbxA = new QDoubleSpinBox;
  _spnbxA->setButtonSymbols(QAbstractSpinBox::NoButtons);
  _spnbxA->setAlignment(Qt::AlignHCenter);
  _spnbxA->setRange(-1000, 1000);
  _spnbxA->setValue(1.0);

  _spnbxB = new QDoubleSpinBox;
  _spnbxB->setButtonSymbols(QAbstractSpinBox::NoButtons);
  _spnbxB->setAlignment(Qt::AlignHCenter);
  _spnbxB->setRange(-1000, 1000);
  _spnbxB->setValue(1.0);

  _layout = new QGridLayout;
  _layout->addWidget(_lblA, 0, 0);
  _layout->addWidget(_lblB, 1, 0);
  _layout->addWidget(_spnbxA, 0, 1);
  _layout->addWidget(_spnbxB, 1, 1);
  _layout->addWidget(_sldrA, 0, 2);
  _layout->addWidget(_sldrB, 1, 2);
  this->setLayout(_layout);

  _timerA = new QTimer;
  _timerA->setInterval(200);

  _timerB = new QTimer;
  _timerB->setInterval(200);

  // соединение сигналов-слотов интерфейса
  connect(_timerA, &QTimer::timeout, 
    _spnbxA, [&] { _spnbxA->setValue(_spnbxA->value() + _sldrA->value()); });
  connect(_sldrA, &QSlider::sliderPressed,
    _timerA, static_cast<void (QTimer::*) ()>(&QTimer::start));
  connect(_sldrA, &QSlider::sliderReleased,
    _timerA, static_cast<void (QTimer::*) ()>(&QTimer::stop));
  connect(_sldrA, &QSlider::sliderReleased,
    _sldrA, [&] { _sldrA->setValue(0); });

  connect(_timerB, &QTimer::timeout,
    _spnbxB, [&] { _spnbxB->setValue(_spnbxB->value() + _sldrB->value()); });
  connect(_sldrB, &QSlider::sliderPressed,
    _timerB, static_cast<void (QTimer::*) ()>(&QTimer::start));
  connect(_sldrB, &QSlider::sliderReleased,
    _timerB, static_cast<void (QTimer::*) ()>(&QTimer::stop));
  connect(_sldrB, &QSlider::sliderReleased,
    _sldrB, [&] { _sldrB->setValue(0); });
}

QDoubleSpinBox* TestTaskRemaster::getspnbxA()
{
  return _spnbxA;
}

QDoubleSpinBox* TestTaskRemaster::getspnbxB()
{
  return _spnbxB;
}

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  TestTaskRemaster w;
  w.show();
  viewerThread* myViwer = new viewerThread;
  MyRender* myRender = new MyRender;

  osg::ref_ptr<osg::MatrixTransform> mtLeft = new osg::MatrixTransform;
  osg::Matrix m;
  m.makeTranslate(-30, 0, 0);
  mtLeft->setMatrix(m);
  mtLeft->addChild(myRender);

  osg::ref_ptr<osg::MatrixTransform> mtRight = new osg::MatrixTransform;
  m.makeTranslate(30, 0, 0);
  mtRight->setMatrix(m);
  mtRight->addChild(myRender);

  osg::ref_ptr<osg::Group> myGroup = new osg::Group;
  myGroup->addChild(mtLeft);
  myGroup->addChild(mtRight);

  // сигналы-слоты для установки параметров А, В
  QObject::connect(w.getspnbxA(), static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    myRender, &MyRender::argA);
  QObject::connect(w.getspnbxB(), static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    myRender, &MyRender::argB);
  myViwer->_viewer->setSceneData(myGroup);
  myViwer->start();
  return a.exec();
}