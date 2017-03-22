#pragma once

#include <QtWidgets>
#include <osg/Geode>
#include <osgViewer/Viewer>
#include <osg/Geometry>
#include <osg/ref_ptr>
#include <osg/MatrixTransform>
#include <QThread>
#include <mutex>

class viewerThread : public QThread, public osg::Referenced
{
public:
  viewerThread();
  osg::ref_ptr<osgViewer::Viewer> _viewer;
  void run() override;
};

class ndCallback : public QObject, public osg::NodeCallback
{
  Q_OBJECT
signals:
  void timeIsGoing(double t);
public slots:
  void reRender(osg::ref_ptr<osg::Vec3Array> v, osg::ref_ptr<osg::Vec3Array> n);
public:
  void operator()(osg::Node*, osg::NodeVisitor*);
private:
  osg::ref_ptr<osg::Vec3Array> _v;
  osg::ref_ptr<osg::Vec3Array> _n;
  std::mutex _mutex;
};

class MyMath : public QThread
{
  Q_OBJECT
public:
  MyMath(osg::ref_ptr<ndCallback>);
  void run() override;
signals:
  void workFinish(osg::ref_ptr<osg::Vec3Array> v, osg::ref_ptr<osg::Vec3Array> n);
public slots:
  void workBegin(double a, double b, double t);
  void setA(double a);
  void setB(double b);
  void setTime(double t);
private:
  double _a;
  double _b;
  double _t;
  std::mutex _mutex;
  std::condition_variable _condVar;
  bool _needUpdate;
};

class MyRender : public QObject, public osg::Group
{
  Q_OBJECT
public:
  MyRender();
  void argA(double);
  void argB(double);
private:
  osg::ref_ptr<osg::Geometry> _geom;
  osg::ref_ptr<osg::Geode> _top;
  osg::ref_ptr<osg::MatrixTransform> _bottomMT;
  osg::ref_ptr<ndCallback> _myCallback;
  MyMath* _myMath;
};


class TestTaskRemaster : public QWidget
{
  Q_OBJECT
public:
  TestTaskRemaster(QWidget *parent = Q_NULLPTR);
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
