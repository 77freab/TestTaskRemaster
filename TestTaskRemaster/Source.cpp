#include "main.h"
#include <osg/array>
#include <osgViewer/ViewerEventHandlers>
#include <QDebug>
#include <osgGA/OrbitManipulator>

Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

const int XY = 20; // расстояние прорисовки графика от центра
const double RES = 1; // разрешение на графике

// Возвращает нормаль полигона (направление, куда повернут полигон)
osg::Vec3 Normal(osg::Vec3 t1, osg::Vec3 t2, osg::Vec3 t3);

double func(double a, double b, double x, double y, double t)
{
  double divisor = a*a*b*b*sin(t)*sin(t); // если деление на 0 возвращаем 0
  return divisor != 0 ? sqrt( ((a*a*sin(t)*sin(t)*x*x+b*b+y*y) / (divisor)) + 1 ) : 0;
};

viewerThread::viewerThread()
  : _viewer(new osgViewer::Viewer)
{
  _viewer->setUpViewInWindow(200, 400, 800, 600);
  _viewer->addEventHandler(new osgViewer::StatsHandler());
  
  //_viewer->getCamera()->setProjectionMatrixAsPerspective(45.0, 1.0, 0.5, 1000);
  //_viewer->getCamera()->setViewMatrix(osg::Matrix::lookAt(
   // osg::Vec3(0, -2000, 0), osg::Vec3(0, 0, 0), osg::Vec3(0, 0, 1)));
  //_vwr->setRunMaxFrameRate(1);
}

// вьювер запускается в отдельном потоке
void viewerThread::run()
{
  _viewer->run();
}

MyRender::MyRender()
  : _geom(new osg::Geometry), _myCallback(new ndCallback), _myMath(new MyMath(_myCallback))
{
  // сигнал-слот для установки времени
  connect(_myCallback, &ndCallback::timeIsGoing,
    _myMath, &MyMath::setTime, Qt::DirectConnection);

  _myMath->start(); // запуск потока мат. вычислений

  //osg::ref_ptr<osg::Vec3Array> n = new osg::Vec3Array;
  //n->setBinding(osg::Array::BIND_OVERALL);
  //_geom->setNormalArray(n);
  //n->push_back(osg::Vec3(0.f, 0.f, 1.f));

  // цвет
  osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
  _geom->setColorArray(c);
  _geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  // добавляем все примитивы и цвета
  for (int i = 0; i < (((4 * XY) / RES) + 2) * ((2 * XY) / RES) * 2; i += ((4 * XY)/ RES) + 2) //(((4 * XY)/ RES) + 2) - точек в страйпе, (((4 * XY)/ RES) + 2) * ((2 * XY) / RES) - всего точек в 1 поверхности
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLE_STRIP, i, ((4 * XY)/ RES) + 2  ));

  for (int i = 0; i < (((4 * XY) / RES) + 2) * ((2 * XY) / RES); i++)
  {
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 0.f, 1.f));
  }

  this->addDrawable(_geom);
  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
  setInitialBound(osg::BoundingBox(-20, -20, -20, 20, 20, 20));
}

// слоты соединенные с QDoubleSpinBox
void MyRender::argA(double a)
{
  _myMath->setA(a); // обновляют аргументы используемые в расчетах
}

void MyRender::argB(double b)
{
  _myMath->setB(b);
}

MyMath::MyMath(osg::ref_ptr<ndCallback> callback)
  : _a(1), _b(1), _t(1), _needUpdate(false)
{
  qRegisterMetaType<osg::ref_ptr<osg::Vec3Array>>();
  connect(this, &MyMath::workFinish,  // сигнал-слот завершения расчета и необходимости перерисовки
    callback, &ndCallback::reRender, Qt::DirectConnection);
}

// поток мат. расчетов
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
    workBegin(_a, _b, _t);  // вызов функции для фактического расчета значений
  }
}

// слот для установки значения текущего времени используемого в расчетах
void MyMath::setTime(double t)
{
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _t = t;
    _needUpdate = true;
  }
  _condVar.notify_one();
}

// слоты для установки значения аргументов используемых в расчетах
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

// функция реализующая фактический расчет
void MyMath::workBegin(double a, double b, double t)
{
  osg::ref_ptr<osg::Vec3Array> mathVertices = new osg::Vec3Array;
  osg::ref_ptr<osg::Vec3Array> mathNormals = new osg::Vec3Array;
  mathNormals->setBinding(osg::Array::BIND_PER_VERTEX);
  mathVertices->reserve((((4 * XY) / RES) + 2) * ((2 * XY) / RES) * 2);
  mathNormals->reserve((((4 * XY) / RES) + 2) * ((2 * XY) / RES) * 2);
  float a1, a2, a3;
  //sleep(1);
  for (float x = -XY; x < XY; x += RES)
    for (float y = -XY; y < XY+RES; y += RES)
    {
      // поверхность для тестов - плоскость
      //a1 = _a;
      //a2 = _b;

      // поверхность по заданию
      a1 = func(a, b,       x,       y, t);
      a2 = func(a, b, x + RES,       y, t);
      a3 = func(a, b,       x, y + RES, t);

      mathVertices->push_back(osg::Vec3(      x, y, a1));
      mathVertices->push_back(osg::Vec3(x + RES, y, a2));

      mathNormals->push_back(Normal(
        osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a2), osg::Vec3(x, y + RES, a3)));
      mathNormals->push_back(Normal(
        osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a2), osg::Vec3(x, y + RES, a3)));
    }
  for (float x = -XY; x < XY; x += RES)
    for (float y = -XY; y < XY+RES; y += RES)
    {
      a1 = func(a, b,       x,       y, t);
      a2 = func(a, b, x + RES,       y, t);
      a3 = func(a, b,       x, y + RES, t);

      mathVertices->push_back(osg::Vec3(      x, y, -a1));
      mathVertices->push_back(osg::Vec3(x + RES, y, -a2));

      mathNormals->push_back(Normal(
        osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a2), osg::Vec3(x, y + RES, a3)));
      mathNormals->push_back(Normal(
        osg::Vec3(x, y, a1), osg::Vec3(x + RES, y, a2), osg::Vec3(x, y + RES, a3)));
    }
  emit workFinish(mathVertices, mathNormals);
}

// слот для установки значений векторов вершин и нормалей используемыхпри перерисовке
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
  emit timeIsGoing(ndv->getFrameStamp()->getSimulationTime()); // передача текущего времени
  traverse(nd, ndv);
}