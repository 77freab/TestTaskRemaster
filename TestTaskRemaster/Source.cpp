#include "main.h"
#include <osg/array>
#include <osgViewer\ViewerEventHandlers>

Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

MyRender::MyRender(QDoubleSpinBox* spnbxA, QDoubleSpinBox* spnbxB, osg::ref_ptr<osg::Geode> geode)
  : _geom(new osg::Geometry), _viewer(new osgViewer::Viewer), _myCallback(new ndCallback)
{
  //_myCallback->moveToThread(this);
  _viewer->setUpViewInWindow(200, 400, 800, 600);
  this->start();
  _myMath = new MyMath(_XY, _RES, _myCallback);
  _myMath->moveToThread(&_mathThread);
  connect(spnbxA, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val){ _myMath->workBegin(val, spnbxB->value()); });
  connect(spnbxB, static_cast<void (QDoubleSpinBox::*) (double)>(&QDoubleSpinBox::valueChanged),
    [this, spnbxA, spnbxB](double val) { _myMath->workBegin(spnbxA->value(), val); });
  connect(&_mathThread, &QThread::started,
    [this, spnbxA, spnbxB] { _myMath->workBegin(spnbxA->value(), spnbxB->value()); });
  _mathThread.start();
  
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
  for (int i = 0; i < ((_XY * _XY * 8)*(1 / (_RES * _RES))); i++)
  {
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON, i * 3, 3));
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
  }

  geode->addDrawable(_geom);
  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
  _viewer->setSceneData(geode);
}

void MyRender::run()
{
  _viewer->run();
}

MyRender::~MyRender()
{
  _mathThread.quit();
  _mathThread.wait();
  delete _myMath;
}

MyMath::MyMath(int XY, double RES, osg::ref_ptr<ndCallback> callback)
  : _XY(XY), _RES(RES)
{
  qRegisterMetaType<osg::ref_ptr<osg::Vec3Array>>();
  connect(this, &MyMath::workFinish,
    callback, &ndCallback::reRender);
}

void MyMath::workBegin(double a, double b)
{
  osg::ref_ptr<osg::Vec3Array> mathVec = new osg::Vec3Array;
  float a1, a21, a22, a3, ia1, ia21, ia22, ia3;
  int t = 1;
  for (float x = -_XY; x < _XY; x += _RES)
  {
    for (float y = -_XY; y < _XY; y += _RES)
    {
      a1 = a21 = a;
      a22 = a3 = b;

      //a1  = x*x - y*y;
      //a21 = x*x - (y + res)*(y + res);
      //a22 = (x + res)*(x + res) - y*y;
      //a3  = (x + res)*(x + res) - (y + res)*(y + res);

      //a1  = func(a, b,       x,       y, t);
      //a21 = func(a, b,       x, y + res, t);
      //a22 = func(a, b, x + res,       y, t);
      //a3  = func(a, b, x + res, y + res, t);

      //ia1  = -a1;
      //ia21 = -a21;
      //ia22 = -a22;
      //ia3  = -a3;

      mathVec->push_back(osg::Vec3(x, y, a1));//1
      mathVec->push_back(osg::Vec3(x, y + _RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, a22));//2-2

      mathVec->push_back(osg::Vec3(x + _RES, y + _RES, a3));//3
      mathVec->push_back(osg::Vec3(x, y + _RES, a21));//2-1
      mathVec->push_back(osg::Vec3(x + _RES, y, a22));//2-2
    }
  }
  emit workFinish(mathVec);
}

void ndCallback::reRender(osg::ref_ptr<osg::Vec3Array> v)
{
  _v = v;
}

void ndCallback::operator()(osg::Node* nd, osg::NodeVisitor* ndv)
{
  osg::Geometry* geom = dynamic_cast<osg::Geometry*>(nd);
  geom->setVertexArray(_v);
  traverse(nd, ndv);
}