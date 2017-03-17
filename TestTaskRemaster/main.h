#pragma once

#include <QtWidgets>
#include <osg/Geode>
#include <osgViewer/Viewer>
#include <osg/Geometry>
#include <osg/ref_ptr>
#include <QThread>
#include <mutex>

class ndCallback : public QObject, public osg::NodeCallback
{
  Q_OBJECT
public slots:
  void reRender(osg::ref_ptr<osg::Vec3Array>);
public:
  void operator()(osg::Node*, osg::NodeVisitor*);
private:
  osg::ref_ptr<osg::Vec3Array> _v;
  std::mutex _mutex;
};

class MyMath : public QThread
{
  Q_OBJECT
public:
  MyMath(const int, const double, osg::ref_ptr<ndCallback>);
  void run() override;
signals:
  void workFinish(osg::ref_ptr<osg::Vec3Array>);
public slots:
  void workBegin(double a, double b);
  void setArgs(double a, double b);
private:
  double _a;
  double _b;
  int _XY;
  double _RES;
  std::mutex _mutex;
  std::condition_variable _condVar;
  bool _needUpdate;
};

class MyRender : public QThread, public osg::Geode
{
  Q_OBJECT
public:
  MyRender(QDoubleSpinBox*, QDoubleSpinBox*);//, osg::ref_ptr<osg::Geode>);
  void run() override;
  ~MyRender();
private:
  const int _XY = 20;
  const double _RES = 1;
  osg::ref_ptr<osgViewer::Viewer> _viewer;
  osg::ref_ptr<osg::Geometry> _geom;
  osg::ref_ptr<ndCallback> _myCallback;
  MyMath* _myMath;
};


class TestTaskRemaster : public QWidget
{
  Q_OBJECT
public:
  TestTaskRemaster(QWidget *parent = Q_NULLPTR);
  ~TestTaskRemaster();
  QDoubleSpinBox* getspnbxA();
  QDoubleSpinBox* getspnbxB();
private:
  QLabel* _lblA;
  QLabel* _lblB;
  QSlider* _sldrA;
  QSlider* _sldrB;
  QDoubleSpinBox* _spnbxA;
  QDoubleSpinBox* _spnbxB;
  QGridLayout* _layout;
  QTimer* _timerA;
  QTimer* _timerB;
};
