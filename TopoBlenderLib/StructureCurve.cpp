#include "StructureCurve.h"
#include "LineSegment.h"
using namespace Structure;

Curve::Curve(const NURBSCurve & newCurve, QString newID, QColor color)
{
    this->curve = newCurve;
    this->id = newID;

    this->vis_property["color"] = color;
    this->vis_property["showControl"] = false;
}

Node * Curve::clone()
{
	Curve * cloneCurve = new Curve( this->curve, this->id );
	return cloneCurve;
}

QString Curve::type()
{
    return CURVE;
}

QBox3D Curve::bbox(double scaling)
{
    QBox3D box;

    foreach(Vec3d cp, curve.getControlPoints())
        box.unite(cp);

	// Scaling
	QVector3D diagonal = box.size() * 0.5;
	box.unite(box.center() + (diagonal * scaling));
	box.unite(box.center() - (diagonal * scaling));

    return box;
}

std::vector<int> Curve::controlCount()
{
	return std::vector<int>( 1, curve.GetNumCtrlPoints() );
}

std::vector<Vector3> Curve::controlPoints()
{
	return curve.getControlPoints();
}

std::vector<Scalar> Curve::controlWeights()
{
	return curve.getControlWeights();
}

void Curve::get( const Vec4d& coordinates, Vector3 & pos, std::vector<Vector3> & frame )
{
	double u = coordinates[0];
	Vector3 der1(0);

	frame.resize(3, Vector3(0));

	curve.GetFrame(u, pos, frame[0], frame[1], frame[2]);
}

SurfaceMeshTypes::Vector3 Curve::position( const Vec4d& coordinates )
{
	Vector3 p(0); get(coordinates,p);
	return p;
}

Vec4d Curve::approxCoordinates( const Vector3 & pos )
{
	Scalar t = curve.timeAt( pos );
	return Vec4d( t, 0, 0, 0 );
}

SurfaceMeshTypes::Vector3 Curve::approxProjection( const Vector3 & point )
{
	Vec4d coords = approxCoordinates(point);
	return curve.GetPosition(coords[0]);
}

std::vector< std::vector<Vector3> > Curve::discretized(Scalar resolution)
{
	return curve.toSegments( resolution );
}

std::vector< std::vector<Vector3> > Structure::Curve::discretizedPoints( Scalar resolution )
{
	std::vector< std::vector<Vector3> > result;

	Scalar curveLength = curve.GetLength(0,1);

	// For singular cases
	if(curveLength < resolution){
		result.push_back(curve.mCtrlPoint);
		return result;
	}

	int np = 1 + (curveLength / resolution);
	std::vector<Vector3> pts;

	curve.SubdivideByLength(np, pts);

	result.push_back(pts);
	return result;
}

void Structure::Curve::laplacianSmoothControls( int num_iterations, std::set<int> anchored )
{
	std::vector<Vector3> & cpnts = curve.mCtrlPoint;

	// Special case anchoring
	if(anchored.count(-1)){
		anchored.clear();		
		anchored.insert(0);
		anchored.insert(cpnts.size() - 1);
	}

	// Laplacian smoothing
	for(int itr = 0; itr < num_iterations; itr++)
	{
		std::vector<Vector3> newCtrlPnts = cpnts;

		for (int j = 0; j < (int)cpnts.size(); j++)
		{
			if(anchored.count(j) == 0)
				newCtrlPnts[j] = (cpnts[j-1] + cpnts[j+1]) / 2.0;
		}

		for (int j = 0; j < (int)cpnts.size(); j++)
			cpnts[j] = newCtrlPnts[j];
	}
}

Vector3 & Curve::controlPoint( int idx )
{
	return curve.mCtrlPoint[idx];
}

int Curve::controlPointIndexFromCoord( Vec4d coord )
{
	// Get point at these coordinates
	/*Vector3 pos(0); curve.Get(coord[0], &pos, 0,0,0);

	int minIdx = -1;
	double minDist = DBL_MAX;

	for(int i = 0; i < (int) curve.mCtrlPoint.size(); i++){
		Vector3 & cp = curve.mCtrlPoint[i];
		double dist = (pos - cp).norm();
		if(dist < minDist){
			minDist = dist;
			minIdx = i;
		}
	}*/

	return (curve.mCtrlPoint.size() - 1) * coord[0];
}

Vector3 & Curve::controlPointFromCoord( Vec4d coord )
{
	return curve.mCtrlPoint[controlPointIndexFromCoord( coord )];
}

SurfaceMeshTypes::Scalar Curve::area()
{
	double a = 0;

	std::vector<Vector3> pnts;
	curve.SubdivideByLength(10, pnts);

	for(int i = 0; i < (int)pnts.size() - 1; i++)
		a += (pnts[i+1] - pnts[i]).norm();

	return a;
}

SurfaceMeshTypes::Vector3 Curve::center()
{
	Vector3 pos(0);
	get(Vec4d(0.5,0.5,0,0), pos);
	return pos;
}

void Curve::draw()
{
    NURBS::CurveDraw::draw( &curve, vis_property["color"].value<QColor>(), vis_property["showControl"].toBool() );
}