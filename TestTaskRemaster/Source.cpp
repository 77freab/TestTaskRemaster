#include "main.h"
#include <osg/array>
#include <osgViewer\ViewerEventHandlers>

Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

double func(double a, double b, double x, double y, double t)
{
  double loh = a*a*b*b*sin(t)*sin(t);
  return loh != 0 ? sqrt(((a*a*sin(t)*sin(t)*x*x+b*b+y*y) / (loh)) + 1) : 0;
};

MyRender::MyRender(QDoubleSpinBox* spnbxA, QDoubleSpinBox* spnbxB)//, osg::ref_ptr<osg::Geode> geode)
  : _geom(new osg::Geometry), _viewer(new osgViewer::Viewer), _myCallback(new ndCallback)
{
  //_myCallback->moveToThread(this);
  _viewer->setUpViewInWindow(200, 400, 800, 600);
  _viewer->addEventHandler(new osgViewer::StatsHandler());
  _myMath = new MyMath(_XY, _RES, _myCallback);
  //_myMath->moveToThread(&_mathThread);
  connect(spnbxA, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val){ _myMath->setArgs(val, spnbxB->value()); });
  connect(spnbxB, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val) { _myMath->setArgs(spnbxA->value(), val); });
  connect(_myMath, &QThread::started,
    [this, spnbxA, spnbxB] { _myMath->setArgs(spnbxA->value(), spnbxB->value()); });
  _myMath->start();
  
  // нормали
  osg::ref_ptr<osg::Vec3Array> n = new osg::Vec3Array;
  _geom->setNormalArray(n);
  _geom->setNormalBinding(osg::Geometry::BIND_OVERALL);
  n->push_back(osg::Vec3(0.f, 0.f, 1.f));

  // цвет
  osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
  _geom->setColorArray(c);
  _geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  // добавляем все полигоны и цвета
  for (int i = 0; i < ((_XY * _XY * 8)*(1 / (_RES * _RES)))*2; i++)
  {
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON, i * 3, 3));
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
  }

  this->addDrawable(_geom);
  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
  _viewer->setSceneData(this);
}

void MyRender::run()
{
  _viewer->run();
}

MyRender::~MyRender()
{
  delete _myMath;
}

MyMath::MyMath(int XY, double RES, osg::ref_ptr<ndCallback> callback)
  : _XY(XY), _RES(RES), _a(1), _b(1), _needUpdate(false)
{
  qRegisterMetaType<osg::ref_ptr<osg::Vec3Array>>();
  connect(this, &MyMath::workFinish,
    callback, &ndCallback::reRender);
}

void MyMath::run()
{
  while (true)
  {
    double a, b;
    {
      std::unique_lock<std::mutex> lock(_mutex);
      while (!_needUpdate)
      {
        _condVar.wait(lock);
      }
      a = _a;
      b = _b;
      _needUpdate = false;
    }
    workBegin(a, b);
  }
}

void MyMath::setArgs(double a, double b)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _a = a;
    _b = b;
    _needUpdate = true;
  }
  _condVar.notify_one();
}

void MyMath::workBegin(double a, double b)//, double t)
{
  osg::ref_ptr<osg::Vec3Array> mathVec = new osg::Vec3Array;
  float a1, a21, a22, a3;
  int t = 1;
  for (float x = -_XY; x < _XY; x += _RES)
  {
    for (float y = -_XY; y < _XY; y += _RES)
    {
      //a1 = a21 = a;
      //a22 = a3 = b;

      //a1  = x*x - y*y;
      //a21 = x*x - (y + _RES)*(y + _RES);
      //a22 = (x + _RES)*(x + _RES) - y*y;
      //a3  = (x + _RES)*(x + _RES) - (y + _RES)*(y + _RES);

      a1  = func(a, b,        x,        y, t);
      a21 = func(a, b,        x, y + _RES, t);
      a22 = func(a, b, x + _RES,        y, t);
      a3  = func(a, b, x + _RES, y + _RES, t);

      mathVec->push_back(osg::Vec3(x, y, a1));//1
      mathVec->push_back(osg::Vec3(x, y + _RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, a22));//2-2

      mathVec->push_back(osg::Vec3(x + _RES, y + _RES, a3));//3
      mathVec->push_back(osg::Vec3(x, y + _RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, a22));//2-2

      mathVec->push_back(osg::Vec3(x, y, -a1));//1
      mathVec->push_back(osg::Vec3(x, y + _RES, -a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, -a22));//2-2

      mathVec->push_back(osg::Vec3(x + _RES, y + _RES, -a3));//3
      mathVec->push_back(osg::Vec3(x, y + _RES, -a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, -a22));//2-2
    }
  }
  emit workFinish(mathVec);
}

void ndCallback::reRender(osg::ref_ptr<osg::Vec3Array> v)
{
  std::unique_lock<std::mutex> lock(_mutex);
  _v = v;
}

void ndCallback::operator()(osg::Node* nd, osg::NodeVisitor* ndv)
{
  osg::Geometry* geom = dynamic_cast<osg::Geometry*>(nd);
  {
    std::unique_lock<std::mutex> lock(_mutex);
    geom->setVertexArray(_v);
  }
  traverse(nd, ndv);
}