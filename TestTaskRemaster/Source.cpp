#include "main.h"
#include <osg/array>
#include <osgViewer/ViewerEventHandlers>
#include <QDebug>
#include <osgGA\OrbitManipulator>

Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

const int XY = 20; // рассто€ние прорисовки графика от центра
const double RES = 1; // разрешение на графике

// ¬озвращает нормаль полигона (направление, куда повернут полигон)
osg::Vec3 Normal(osg::Vec3 t1, osg::Vec3 t2, osg::Vec3 t3);

double func(double a, double b, double x, double y, double t)
{
  double divisor = a*a*b*b*sin(t)*sin(t); // если деление на 0 возвращаем 0
  return divisor != 0 ? sqrt( ((a*a*sin(t)*sin(t)*x*x+b*b+y*y) / (divisor)) + 1 ) : 0;
};

viewerThread::viewerThread()
{
  _viewer = new osgViewer::Viewer;
  _viewer->setUpViewInWindow(200, 400, 800, 600);
  _viewer->addEventHandler(new osgViewer::StatsHandler());
  auto manip = new osgGA::OrbitManipulator();
  _viewer->setCameraManipulator(manip);
  manip->setDistance(2000);
  
  //_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, 1.0, 0.5, 1000);
  //_viewer->getCamera()->setViewMatrix(osg::Matrix::lookAt(
   // osg::Vec3(0, -2000, 0), osg::Vec3(0, 0, 0), osg::Vec3(0, 0, 1)));
  //_vwr->setRunMaxFrameRate(1);
}

void viewerThread::run() // вьювер запускаетс€ в отдельном потоке
{
  _viewer->run();
}

MyRender::MyRender()
  : _geom(new osg::Geometry), _myCallback(new ndCallback), _myMath(new MyMath(_myCallback))
{
  // сигнал-слот дл€ установки времени
  connect(_myCallback, &ndCallback::timeIsGoing,
    _myMath, &MyMath::setTime, Qt::DirectConnection);

  _myMath->start(); // запуск потока мат. вычислений
  
  //_geom->setNormalBinding(osg::Geometry::BIND_PER_PRIMITIVE_SET);

  // цвет
  osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
  _geom->setColorArray(c);
  _geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  // добавл€ем все полигоны и цвета
  for (int i = 0; i < ((XY * XY * 16)*(1 / (RES * RES))); i++)
  {
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, i * 3, 3));
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
  }

  this->addDrawable(_geom);
  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
}

void MyRender::argA(double a)
{
  _myMath->setA(a);
}

void MyRender::argB(double b)
{
  _myMath->setB(b);
}

MyMath::MyMath(osg::ref_ptr<ndCallback> callback)
  : _a(1), _b(1), _t(1), _needUpdate(false)
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
    workBegin(_a, _b, _t);
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

void MyMath::setA(double a)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _a = a;
    _needUpdate = true;
  }
  _condVar.notify_one();
}

void MyMath::setB(double b)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _b = b;
    _needUpdate = true;
  }
  _condVar.notify_one();
}

void MyMath::workBegin(double a, double b, double t)
{
  osg::ref_ptr<osg::Vec3Array> mathVertices = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec3Array> mathNormals = new osg::Vec3Array;
  mathNormals->setBinding(osg::Array::BIND_PER_PRIMITIVE_SET);
  mathVertices->reserve((XY * XY * 16)*(1 / (RES * RES))*3);
  mathNormals->reserve((XY * XY * 16)*(1 / (RES * RES)));
  float a1, a21, a22, a3;
  //sleep(1);
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

      a1  = func(a, b,       x,       y, t);
      a21 = func(a, b,       x, y + RES, t);
      a22 = func(a, b, x + RES,       y, t);
      a3  = func(a, b, x + RES, y + RES, t);

      mathVertices->push_back(osg::Vec3(      x,       y, a1));//1
      mathVertices->push_back(osg::Vec3(      x, y + RES, a21));//2-1
      mathVertices->push_back(osg::Vec3(x + RES,       y, a22));//2-2
      mathNormals->push_back(Normal(
        osg::Vec3(x, y + RES, a21), osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a22))); // y, x, z

      mathVertices->push_back(osg::Vec3(x + RES, y + RES, a3));//3
      mathVertices->push_back(osg::Vec3(      x, y + RES, a21));//2-1
      mathVertices->push_back(osg::Vec3(x + RES,       y, a22));//2-2
      mathNormals->push_back(Normal(
        osg::Vec3(x + RES, y + RES, a3), osg::Vec3(x, y + RES, a21), osg::Vec3(x + RES, y, a22))); // x, y, z

      mathVertices->push_back(osg::Vec3(      x,       y, -a1));//1
      mathVertices->push_back(osg::Vec3(      x, y + RES, -a21));//2-1
      mathVertices->push_back(osg::Vec3(x + RES,       y, -a22));//2-2
      mathNormals->push_back(Normal(
        osg::Vec3(x, y + RES, a21), osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a22))); // y, x, z

      mathVertices->push_back(osg::Vec3(x + RES, y + RES, -a3));//3
      mathVertices->push_back(osg::Vec3(      x, y + RES, -a21));//2-1
      mathVertices->push_back(osg::Vec3(x + RES,       y, -a22));//2-2
      mathNormals->push_back(Normal(
        osg::Vec3(x + RES, y + RES, a3), osg::Vec3(x, y + RES, a21), osg::Vec3(x + RES, y, a22))); // x, y, z
    }
  }
  emit workFinish(mathVertices, mathNormals);
}

void ndCallback::reRender(osg::ref_ptr<osg::Vec3Array> v, osg::ref_ptr<osg::Vec3Array> n)
{
  std::unique_lock<std::mutex> lock(_mutex);
  _v = v;
  _n = n;
}

void ndCallback::operator()(osg::Node* nd, osg::NodeVisitor* ndv)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    nd->asGeometry()->setVertexArray(_v);
    nd->asGeometry()->setNormalArray(_n);
  }
  emit timeIsGoing(ndv->getFrameStamp()->getSimulationTime());
  traverse(nd, ndv);
}