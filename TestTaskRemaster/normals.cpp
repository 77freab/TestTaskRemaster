#include <main.h>

// Возвращает вектор между 2мя точками.
osg::Vec3 Vector(osg::Vec3 vPoint1, osg::Vec3 vPoint2)
{
  osg::Vec3 vVector;

  vVector[0] = vPoint1[0] - vPoint2[0];
  vVector[1] = vPoint1[1] - vPoint2[1];
  vVector[2] = vPoint1[2] - vPoint2[2];

  return vVector;
}

// Возвращает вектор, перпендикулярный 2м переданным.
osg::Vec3 Cross(osg::Vec3 vVector1, osg::Vec3 vVector2)
{
  osg::Vec3 vNormal;

  vNormal[0] = ((vVector1[1] * vVector2[2]) - (vVector1[2] * vVector2[1]));
  vNormal[1] = ((vVector1[2] * vVector2[0]) - (vVector1[0] * vVector2[2]));
  vNormal[2] = ((vVector1[0] * vVector2[1]) - (vVector1[1] * vVector2[0]));

  return vNormal;
}

// Возвращает нормаль полигона
osg::Vec3 Normal(osg::Vec3 t1, osg::Vec3 t2, osg::Vec3 t3)
{
  osg::Vec3 vVector1 = Vector(t3, t1); // q
  osg::Vec3 vVector2 = Vector(t2, t1); // p

  osg::Vec3 vNormal = Cross(vVector1, vVector2);
  
  vNormal.normalize();

  return vNormal;
}