#include "main.h"
#include <osg/array>
#include <osgViewer/ViewerEventHandlers>


Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

const int XY = 20;
const double RES = 1;

double func(double a, double b, double x, double y, double t)
{
  double divisor = a*a*b*b*sin(t)*sin(t);
  return divisor != 0 ? sqrt( ((a*a*sin(t)*sin(t)*x*x+b*b+y*y) / (divisor)) + 1 ) : 0;
};

MyViewer::MyViewer()
{
  _vwr = new osgViewer::Viewer;
  _vwr->setUpViewInWindow(200, 400, 800, 600);
  _vwr->addEventHandler(new osgViewer::StatsHandler());
}

void MyViewer::run()
{
  _vwr->osgViewer::Viewer::run();
}

MyRender::MyRender(osg::ref_ptr<osgViewer::Viewer> viewer, QDoubleSpinBox* spnbxA, QDoubleSpinBox* spnbxB)//, osg::ref_ptr<osg::Geode> geode)
  : _geom(new osg::Geometry), _myCallback(new ndCallback)
{
  _myMath = new MyMath(_myCallback);
  connect(spnbxA, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val){ _myMath->setArgs(val, spnbxB->value()); });
  connect(spnbxB, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val) { _myMath->setArgs(spnbxA->value(), val); });

  connect(_myCallback, &ndCallback::timeIsGoing,
    _myMath, &MyMath::setTime);

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
  for (int i = 0; i < ((XY * XY * 8)*(1 / (RES * RES)))*2; i++)
  {
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON, i * 3, 3));
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
  }

  this->addDrawable(_geom);
  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
  viewer->setSceneData(this);
}

MyMath::MyMath(osg::ref_ptr<ndCallback> callback)
  : _a(1), _b(1), _needUpdate(false)
{
  qRegisterMetaType<osg::ref_ptr<osg::Vec3Array>>();
  connect(this, &MyMath::workFinish,
    callback, &ndCallback::reRender);
}

void MyMath::run()
{
  while (true)
  {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      while (!_needUpdate)
      {
        _condVar.wait(lock);
      }
      _needUpdate = false;
    }
    workBegin();
  }
}

void MyMath::setTime(double t)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _t = t;
    _needUpdate = true;
  }
  _condVar.notify_one();
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

void MyMath::workBegin()
{
  osg::ref_ptr<osg::Vec3Array> mathVec = new osg::Vec3Array;
  float a1, a21, a22, a3;
  for (float x = -XY; x < XY; x += RES)
  {
    for (float y = -XY; y < XY; y += RES)
    {
      //a1 = a21 = _a;
      //a22 = a3 = _b;

      //a1  = x*x - y*y;
      //a21 = x*x - (y + RES)*(y + RES);
      //a22 = (x + RES)*(x + RES) - y*y;
      //a3  = (x + RES)*(x + RES) - (y + RES)*(y + RES);

      a1  = func(_a, _b,       x,       y, _t);
      a21 = func(_a, _b,       x, y + RES, _t);
      a22 = func(_a, _b, x + RES,       y, _t);
      a3  = func(_a, _b, x + RES, y + RES, _t);

      mathVec->push_back(osg::Vec3(      x,       y, a1));//1
      mathVec->push_back(osg::Vec3(      x, y + RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + RES,       y, a22));//2-2

      mathVec->push_back(osg::Vec3(x + RES, y + RES, a3));//3
      mathVec->push_back(osg::Vec3(      x, y + RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + RES,       y, a22));//2-2

      mathVec->push_back(osg::Vec3(      x,       y, -a1));//1
      mathVec->push_back(osg::Vec3(      x, y + RES, -a21));//2-1
      mathVec->push_back(osg::Vec3(x + RES,       y, -a22));//2-2

      mathVec->push_back(osg::Vec3(x + RES, y + RES, -a3));//3
      mathVec->push_back(osg::Vec3(      x, y + RES, -a21));//2-1
      mathVec->push_back(osg::Vec3(x + RES,       y, -a22));//2-2
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
  {
    std::unique_lock<std::mutex> lock(_mutex);
    nd->asGeometry()->setVertexArray(_v);
  }
  emit timeIsGoing(ndv->getFrameStamp()->getSimulationTime());
  traverse(nd, ndv);
}