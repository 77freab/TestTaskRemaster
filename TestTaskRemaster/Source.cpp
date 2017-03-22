#include "main.h"
#include <osg/array>
#include <osgViewer/ViewerEventHandlers>
#include <QDebug>
#include <osgGA/OrbitManipulator>

Q_DECLARE_METATYPE(osg::ref_ptr<osg::Vec3Array>)

const int XY = 20; // расстояние прорисовки графика от центра
const float RES = 1; // разрешение на графике
const int NumVerticesInStripe = ((4 * XY) / RES) + 2; // кол-во вершин в страйпе
const int NumVerticesInSurfase = NumVerticesInStripe * ((2 * XY) / RES); // кол-во вершин на одной поверхности

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
  //_vwr->setRunMaxFrameRate(1); // ограничение кол-ва кадров в сек.
}

// вьювер запускается в отдельном потоке
void viewerThread::run()
{
  _viewer->run();
}

MyRender::MyRender()
  : _geom(new osg::Geometry), _myCallback(new ndCallback), _myMath(new MyMath(_myCallback)),
  _top(new osg::Geode), _bottomMT(new osg::MatrixTransform)
{
  // сигнал-слот для установки времени
  connect(_myCallback, &ndCallback::timeIsGoing,
    _myMath, &MyMath::setTime, Qt::DirectConnection);

  _myMath->start(); // запуск потока мат. вычислений

  // цвет
  osg::ref_ptr<osg::Vec4Array> c = new osg::Vec4Array;
  _geom->setColorArray(c);
  _geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

  // добавляем все примитивы и цвета
  for (int i = 0; i < NumVerticesInSurfase; i += NumVerticesInStripe)
    _geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLE_STRIP, i, NumVerticesInStripe));

  for (int i = 0; i < NumVerticesInSurfase; i++)
  {
    c->push_back(osg::Vec4(1.f, 0.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 1.f, 0.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 1.f, 1.f));
    c->push_back(osg::Vec4(0.f, 0.f, 0.f, 1.f));
  }

  _geom->setDataVariance(osg::Object::DYNAMIC);
  _geom->setUpdateCallback(_myCallback);
  setInitialBound(osg::BoundingBox(-20, -20, -20, 20, 20, 20));
  _top->addDrawable(_geom);

  // нижняя плоскость разворачивается и перемещается
  osg::Matrix mT, mR;
  mT.makeTranslate(0, 0, 3.64191866);
  mR.makeRotate(osg::PI, osg::Vec3(0, 1, 0));
  _bottomMT->setMatrix(mT * mR);
  _bottomMT->addChild(_top);

  this->addChild(_top);
  this->addChild(_bottomMT);
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
  mathVertices->reserve(NumVerticesInSurfase * 2);
  mathNormals->reserve(NumVerticesInSurfase * 2);
  float z1, z2, z3; // координаты z для точек
  osg::Vec3 t1, t2, t3, n; // точки и нормаль
  //sleep(1);
  for (float x = -XY; x < XY; x += RES)
    for (float y = -XY; y < XY+RES; y += RES)
    {
      // поверхность для тестов - плоскость
      //a1 = _a;
      //a2 = _b;

      // поверхность по заданию
      z1 = func(a, b,       x,       y, t);
      z2 = func(a, b, x + RES,       y, t);
      z3 = func(a, b,       x, y + RES, t);

      t1 = {       x,       y, z1 };
      t2 = { x + RES,       y, z2 };
      t3 = {       x, y + RES, z3 };

      n = (t3 - t1) ^ (t2 - t1); // вычисляем нормали
      n.normalize();

      mathVertices->push_back(t1);
      mathVertices->push_back(t2);

      mathNormals->push_back(n);
      mathNormals->push_back(n);
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