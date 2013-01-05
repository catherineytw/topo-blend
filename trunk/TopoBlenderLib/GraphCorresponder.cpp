#include "GraphCorresponder.h"

#include <algorithm>
#include <fstream>

#define INVALID_VALUE -1

GraphCorresponder::GraphCorresponder( Structure::Graph *source, Structure::Graph *target )
{
	this->sg = source;
	this->tg = target;

	sIsLandmark.resize(sg->nodes.size(), false);
	tIsLandmark.resize(tg->nodes.size(), false);

	sIsCorresponded.resize(sg->nodes.size(), false);
	tIsCorresponded.resize(tg->nodes.size(), false);
}


void GraphCorresponder::addLandmarks( std::vector<QString> sParts, std::vector<QString> tParts )
{
	// Check if those parts are available
	foreach(QString strID, sParts)
	{
		Structure::Node * node = sg->getNode(strID);
		if (!node)
		{
			qDebug() << "Add landmarks: " << strID << " doesn't exist in the source graph.";
			return;
		}
		int idx = node->property["index"].toInt();
		if (sIsLandmark[idx])
		{
			qDebug() << "Add landmarks: " << strID << " in the source graph has already been corresponded.";
			return;
		}
	}

	foreach(QString strID, tParts)
	{
		Structure::Node * node = tg->getNode(strID);
		if (!node)
		{
			qDebug() << "Add landmarks: " << strID << " doesn't exist in the target graph.";
			return;
		}
		int idx = node->property["index"].toInt();
		if (tIsLandmark[idx])
		{
			qDebug() << "Add landmarks: " << strID << " in the target graph has already been corresponded.";
			return;
		}
	}

	// Mark those parts
	foreach(QString strID, sParts)
	{
		int idx = sg->getNode(strID)->property["index"].toInt();
		sIsLandmark[idx] = true;
	}

	foreach(QString strID, tParts)
	{
		int idx = tg->getNode(strID)->property["index"].toInt();
		tIsLandmark[idx] = true;
	}

	// Store correspondence
	landmarks.push_back(std::make_pair(sParts, tParts));
}


void GraphCorresponder::removeLandmarks( int pos, int n )
{
	if (pos + n > (int)landmarks.size()) return;

	for (int i = pos; i < pos + n; i++)
	{
		VECTOR_PAIR vector2vector = landmarks[i];

		// Update isLandmark
		foreach(QString strID, vector2vector.first)
		{
			int idx = sg->getNode(strID)->property["index"].toInt();
			sIsLandmark[idx] = false;
		}

		foreach(QString strID, vector2vector.second)
		{
			int idx = tg->getNode(strID)->property["index"].toInt();
			tIsLandmark[idx] = false;
		}
	}

	// Erase
	landmarks.erase(landmarks.begin() + pos, landmarks.begin() + pos + n);
}



void GraphCorresponder::computeValidationMatrix()
{
	initializeMatrix<bool>(validM, true);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();

	// Types
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (sg->nodes[i]->type() != tg->nodes[j]->type())
				validM[i][j] = false;
		}
	}

	// Landmarks
	foreach (VECTOR_PAIR vector2vector, landmarks)
	{
		// Convert strID to int id
		std::set<int> sIDs, tIDs;
		foreach (QString strID, vector2vector.first)
			sIDs.insert(sg->getNode(strID)->property["index"].toInt());
		foreach (QString strID, vector2vector.second)
			tIDs.insert(tg->getNode(strID)->property["index"].toInt());

		// Set distances to invalid value
		foreach (int r, sIDs)
			foreach (int c, tIDs)
			validM[r][c] = false;
	}
}


float GraphCorresponder::supInfDistance( std::vector<Vector3> &A, std::vector<Vector3> &B )
{
	float supinfDis = -1;
	for (int i = 0; i < (int)A.size(); i++)
	{
		float infDis = FLT_MAX;
		for (int j = 0; j < (int)B.size(); j++)
		{
			float dis = (A[i] - B[j]).norm();

			if (dis < infDis)
				infDis = dis;
		}

		if (infDis > supinfDis)
			supinfDis = infDis;
	}

	return supinfDis;
}

float GraphCorresponder::HausdorffDistance( std::vector<Vector3> &A, std::vector<Vector3> &B )
{
	float ABDis = supInfDistance(A, B);
	float BADis = supInfDistance(B, A);

	return std::max(ABDis, BADis);
}


void GraphCorresponder::computeHausdorffDistanceMatrix( std::vector< std::vector<float> > & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	// The control points of each node in the target graph
	std::vector< std::vector<Vector3> > targetControlPointsArray;
	foreach (Structure::Node *tNode, this->tg->nodes)
		targetControlPointsArray.push_back(tNode->controlPoints());

	// The Hausdorff distance
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++)
	{
		std::vector<Vector3> sPoints = sg->nodes[i]->controlPoints();
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				std::vector<Vector3> &tPoints = targetControlPointsArray[j];
				M[i][j] = HausdorffDistance(sPoints, tPoints);
			}
		}
	}

	normalizeMatrix(M);
}



void GraphCorresponder::computeSizeDiffMatrix( std::vector< std::vector<float> > & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				float diff;
				if (sg->nodes[i]->type() == Structure::CURVE)
				{
					Structure::Curve *curve1 = (Structure::Curve *) sg->nodes[i];
					Structure::Curve *curve2 = (Structure::Curve *) tg->nodes[j];
					diff = curve1->curve.GetLength(0, 1) - curve2->curve.GetLength(0, 1);
				}
				else
				{
					Structure::Sheet *sheet1 = (Structure::Sheet *) sg->nodes[i];
					Structure::Sheet *sheet2 = (Structure::Sheet *) tg->nodes[j];
					diff = sheet1->area() - sheet2->area();
				}

				M[i][j] = fabsf(diff);
			}
		}
	}

	normalizeMatrix(M);
}



void GraphCorresponder::computeOrientationDiffMatrix( std::vector< std::vector<float> > & M )
{
	initializeMatrix<float>(M, INVALID_VALUE);

	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				Vector3 vec1, vec2;
				if (sg->nodes[i]->type() == Structure::CURVE)
				{
					Structure::Curve *curve1 = (Structure::Curve *) sg->nodes[i];
					Structure::Curve *curve2 = (Structure::Curve *) tg->nodes[j];
					std::vector<Vector3> ctrlPnts1 = curve1->controlPoints();
					std::vector<Vector3> ctrlPnts2 = curve2->controlPoints();

					vec1 = ctrlPnts1.front() - ctrlPnts1.back();
					vec2 = ctrlPnts2.front() - ctrlPnts2.back();
				}
				else
				{
					Structure::Sheet *sSheet = (Structure::Sheet *) sg->nodes[i];
					Structure::Sheet *tSheet = (Structure::Sheet *) tg->nodes[j];
					Array2D_Vector3 sCtrlPoint = sSheet->surface.mCtrlPoint;
					Array2D_Vector3 tCtrlPoint = tSheet->surface.mCtrlPoint;
					Vector3 scenter = sSheet->center();
					Vector3 tcenter = tSheet->center();

					// Get the extreme points.
					Vector3 s00 = sCtrlPoint.front().front();
					Vector3 s01 = sCtrlPoint.front().back();
					Vector3 s10 = sCtrlPoint.back().front();
					Vector3 sU = s10 - s00;
					Vector3 sV = s01 - s00;

					Vector3 t00 = tCtrlPoint.front().front();
					Vector3 t01 = tCtrlPoint.front().back();
					Vector3 t10 = tCtrlPoint.back().front();
					Vector3 tU = t10 - t00;
					Vector3 tV = t01 - t00;

					// Normals
					vec1 = cross(sU, sV);
					vec2 = cross(tU, tV);
				}

				vec1.normalize();
				vec2.normalize();
				M[i][j] = 1.0 - abs(dot(vec1, vec2));
			}
		}
	}

	normalizeMatrix(M);
}


void GraphCorresponder::computeDistanceMatrix()
{
	// Validation matrix
	computeValidationMatrix();

	// Distance matrices
	std::vector< std::vector<float> > hM, sM, oM;
	computeHausdorffDistanceMatrix(hM);
	computeSizeDiffMatrix(sM);
	computeOrientationDiffMatrix(oM);

	// Combine
	disM = hM;
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++) {
		for (int j = 0; j < tN; j++)
		{
			if (validM[i][j])
			{
				disM[i][j] += sM[i][j] + oM[i][j];
			}
		}
	}

	normalizeMatrix(disM);
}

template <class Type>
void GraphCorresponder::initializeMatrix(std::vector< std::vector<Type> > & M, Type value)
{
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	std::vector<Type> tmp(tN, value);
	M.clear();
	M.resize(sN, tmp);
}


void GraphCorresponder::normalizeMatrix(std::vector< std::vector<float> > & M)
{
	float maxDis = -1;

	// Find the maximum value
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (M[i][j] != INVALID_VALUE && M[i][j] > maxDis)
				maxDis = M[i][j];
		}
	}

	// Normalize
	for(int i = 0; i < sN; i++){
		for (int j = 0; j < tN; j++)
		{
			if (M[i][j] != INVALID_VALUE)
				M[i][j] /= maxDis;
		}
	}
}


void GraphCorresponder::visualizePart2PartDistance( int sourceID )
{
	if (disM.empty()) 
		computeDistanceMatrix();

	// The source
	sourceID %= sg->nodes.size();
	for (int i = 0; i < sg->nodes.size(); i++)
	{
		Structure::Node *sNode = sg->nodes[i];
		float color = (i == sourceID)? 1.0 : 0.0;
		sNode->vis_property["color"] = qtJetColorMap(color);
		sNode->vis_property["showControl"] = false;
	}

	// Visualize by jet color
	for (int j = 0; j < tg->nodes.size(); j++)
	{
		Structure::Node *tNode = tg->nodes[j];

		float value;
		if (disM[sourceID][j] == INVALID_VALUE)
			value = 0;
		else
			value = 1 - disM[sourceID][j];
		tNode->vis_property["color"] = qtJetColorMap(value);
		tNode->vis_property["showControl"] = false;
	}
}



bool GraphCorresponder::minElementInMatrix( std::vector< std::vector<float> > &M, int &row, int &column, float &minValue )
{
	if (M.size() == 0 || M[0].size() == 0)
	{
		qDebug()  << "Warning: minElementInMatrix: the input matrix cannot be empty.";
		return false;
	}

	minValue = FLT_MAX;
	for (int i = 0; i < (int)M.size(); i++){
		for (int j = 0; j < (int)M[0].size(); j++)
		{
			if (M[i][j] != INVALID_VALUE && M[i][j] < minValue)
			{
				minValue = M[i][j];
				row = i;
				column = j;
			}
		}
	}

	return minValue != FLT_MAX;
}

void GraphCorresponder::computePartToPartCorrespondences()
{
	// Clear
	correspondences.clear();
	corrScores.clear();

	// Distance matrix
	if (disM.empty()) computeDistanceMatrix();
	std::vector< std::vector<float> > disMatrix = disM;

	// Parameters
	int sN = sg->nodes.size();
	int tN = tg->nodes.size();
	float tolerance = 0.05f;
	float threshold = 0.3f;

	int r, c;
	float minValue;
	while (minElementInMatrix(disMatrix, r, c, minValue))
	{
		if (minValue > threshold) break;

		// source:r <-> target:c
		float upperBound = disMatrix[r][c] + tolerance;

		// Search for "many" in source
		std::vector<int> r_many;
		std::vector<float> r_scores;
		for (int ri = 0; ri < sN; ri++)
		{
			if (ri == r || disMatrix[ri][c] == INVALID_VALUE)
				continue;

			// ri is close to c
			if (disMatrix[ri][c] <= upperBound) 
			{
				r_scores.push_back(disMatrix[ri][c]);
				r_many.push_back(ri);
			}
		}

		// Search for "many" in target
		std::vector<int> c_many;
		std::vector<float> c_scores;
		for (int ci = 0; ci < tN; ci++)
		{
			if (ci == c || disMatrix[r][ci] == INVALID_VALUE)	
				continue;

			// ci is close to r
			if (disMatrix[r][ci] < upperBound) 
			{
				c_scores.push_back(disMatrix[r][ci]);
				c_many.push_back(ci);
			}
		}

		// Results
		std::vector<QString> sVector, tVector;
		sVector.push_back(sg->nodes[r]->id);
		tVector.push_back(tg->nodes[c]->id);
		std::vector<float> scores;
		scores.push_back(minValue);

		if (c_many.size() > r_many.size()) // r <-> {c_i}
		{
			foreach(int ci, c_many)
			{
				tVector.push_back(tg->nodes[ci]->id);

				for (int i = 0; i < sN; i++) // Column c
					disMatrix[i][ci] = INVALID_VALUE;
			}

			scores.insert(scores.end(), c_scores.begin(), c_scores.end());
		}
		else // {r_i} <-> c
		{
			foreach(int ri, r_many)
			{
				sVector.push_back(sg->nodes[ri]->id);

				for (int j = 0; j < tN; j++) // Row r
					disMatrix[ri][j] = INVALID_VALUE;
			}

			scores.insert(scores.end(), r_scores.begin(), r_scores.end());
		}

		

		// Save results
		this->correspondences.push_back(std::make_pair(sVector, tVector));
		this->corrScores.push_back(scores);
		
		// Remove r and c in the disMatrix
		for (int i = 0; i < sN; i++) // Column c
			disMatrix[i][c] = INVALID_VALUE;
		for (int j = 0; j < tN; j++) // Row r
			disMatrix[r][j] = INVALID_VALUE;
	}
}


void GraphCorresponder::correspondTwoNodes( Structure::Node *sNode, Structure::Node *tNode )
{
	if (sNode->type() != tNode->type())
	{
		qDebug() << "Two nodes with different types cannot be corresponded. ";
		return;
	}

	if (sNode->type() == Structure::SHEET)
		correspondTwoSheets((Structure::Sheet*) sNode, (Structure::Sheet*) tNode);
	else
		correspondTwoCurves((Structure::Curve*) sNode, (Structure::Curve*) tNode);
}


void GraphCorresponder::correspondTwoCurves( Structure::Curve *sCurve, Structure::Curve *tCurve )
{
	std::vector<Vector3> sCtrlPoint = sCurve->controlPoints();
	std::vector<Vector3> tCtrlPoint = tCurve->controlPoints();

	// Euclidean for now, can use Geodesic distance instead if need
	Vector3 scenter = sCurve->center();
	Vector3 sfront = sCtrlPoint.front() - scenter;
	Vector3 tcenter = tCurve->center();
	Vector3 tfront = tCtrlPoint.front() - tcenter;
	Vector3 tback = tCtrlPoint.back() - tcenter;

	float f2f = (sfront - tfront).norm();
	float f2b = (sfront - tback).norm();

	if (f2f > f2b)
	{
		// Flip the target
		std::vector<Scalar> tCtrlWeight = tCurve->controlWeights();
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

		NURBSCurve newCurve(tCtrlPoint, tCtrlWeight);
		tCurve->curve = newCurve;

		// Update the coordinates of links
		foreach( Structure::Link * l, tg->getEdges(tCurve->id) )
		{
			l->setCoord(tCurve->id, inverseCoords( l->getCoord(tCurve->id) ));
		}
	}
}

// Helper function
template <class Type>
std::vector<std::vector<Type> > transpose(const std::vector<std::vector<Type> > data) 
{
	// this assumes that all inner vectors have the same size and
	// allocates space for the complete result in advance
	std::vector<std::vector<Type> > result(data[0].size(), std::vector<Type>(data.size()));
	for (int i = 0; i < (int)data[0].size(); i++){
		for (int j = 0; j < (int)data.size(); j++) 
		{
			result[i][j] = data[j][i];
		}
	}

	return result;
}


void GraphCorresponder::correspondTwoSheets( Structure::Sheet *sSheet, Structure::Sheet *tSheet )
{
	// Old properties
	NURBSRectangle &oldRect = tSheet->surface;
	int uDegree = oldRect.GetDegree(0);
	int vDegree = oldRect.GetDegree(1);
	bool uLoop = oldRect.IsLoop(0);
	bool vLoop = oldRect.IsLoop(1);
	bool uOpen = oldRect.IsOpen(0);
	bool vOpen = oldRect.IsOpen(1);
	bool isModified = false;
	bool isUVFlipped = false;

	// Control points and weights
	Array2D_Vector3 sCtrlPoint = sSheet->surface.mCtrlPoint;
	Array2D_Real sCtrlWeight = sSheet->surface.mCtrlWeight;

	Array2D_Vector3 tCtrlPoint = tSheet->surface.mCtrlPoint;
	Array2D_Real tCtrlWeight = tSheet->surface.mCtrlWeight;

	Array2D_Vector3 tCtrlPointNew;
	Array2D_Real tCtrlWeightNew;

	Vector3 scenter = sSheet->center();
	Vector3 tcenter = tSheet->center();

	// Get the extreme points.
	Vector3 s00 = sCtrlPoint.front().front();
	Vector3 s01 = sCtrlPoint.front().back();
	Vector3 s10 = sCtrlPoint.back().front();
	Vector3 sU = s10 - s00;
	Vector3 sV = s01 - s00;

	Vector3 t00 = tCtrlPoint.front().front();
	Vector3 t01 = tCtrlPoint.front().back();
	Vector3 t10 = tCtrlPoint.back().front();
	Vector3 tU = t10 - t00;
	Vector3 tV = t01 - t00;

	// Flip if need
	Vector3 sUV = cross(sU, sV);
	Vector3 tUV = cross(tU, tV);
	if (dot(sUV, tUV) < 0)
	{
		// Reverse the target along u direction
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

		// Update tU
		tU = -tU;
		tUV = -tUV;
		isModified = true;

		// Update the coordinates of links
		foreach( Structure::Link * l, tg->getEdges(tSheet->id) ){
			std::vector<Vec4d> oldCoord = l->getCoord(tSheet->id), newCoord;
			foreach(Vec4d c, oldCoord) newCoord.push_back(Vec4d(1-c[0], c[1], c[2], c[3]));
			l->setCoord(tSheet->id, newCoord);
		}
	}

	// Rotate if need
	Scalar cosAngle = dot(sU.normalized(), tU.normalized());
	Scalar cos45 = sqrtf(2.0) / 2;

	// Do Nothing
	if (cosAngle > cos45)
	{
		tCtrlPointNew = tCtrlPoint;
		tCtrlWeightNew = tCtrlWeight;
	}
	// Rotate 180 degrees
	else if (cosAngle < - cos45)
	{
		//  --> sV				tU
		// |					|
		// sU             tV <--
		// By flipping along both directions
		std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
		std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

		for (int i = 0; i < (int)tCtrlPoint.size(); i++)
		{
			std::reverse(tCtrlPoint[i].begin(), tCtrlPoint[i].end());
			std::reverse(tCtrlWeight[i].begin(), tCtrlWeight[i].end());
		}

		// The new control points and weights
		tCtrlPointNew = tCtrlPoint;
		tCtrlWeightNew = tCtrlWeight;
		isModified = true;

		// Update the coordinates of links
		foreach( Structure::Link * l, tg->getEdges(tSheet->id) ){
			std::vector<Vec4d> oldCoord = l->getCoord(tSheet->id), newCoord;
			foreach(Vec4d c, oldCoord) newCoord.push_back(Vec4d(1-c[0], 1-c[1], c[2], c[3]));
			l->setCoord(tSheet->id, newCoord);
		}
	}
	// Rotate 90 degrees 
	else
	{
		Vector3 stU = cross(sU, tU);
		if (dot(stU, sUV) >= 0)
		{
			//  --> sV		tV
			// |			|
			// sU           --> tU
			// Transpose and reverse along U
			tCtrlPointNew = transpose<Vector3>(tCtrlPoint);
			tCtrlWeightNew = transpose<Scalar>(tCtrlWeight);

			std::reverse(tCtrlPointNew.begin(), tCtrlPointNew.end());
			std::reverse(tCtrlWeightNew.begin(), tCtrlWeightNew.end());

			// Update the coordinates of links
			foreach( Structure::Link * l, tg->getEdges(tSheet->id) ){
				std::vector<Vec4d> oldCoord = l->getCoord(tSheet->id), newCoord;
				foreach(Vec4d c, oldCoord) newCoord.push_back(Vec4d(1- c[1], c[0], c[2], c[3]));
				l->setCoord(tSheet->id, newCoord);
			}
		}
		else
		{
			//  --> sV	tU<--
			// |			 |
			// sU			tV
			// Reverse along U and Transpose
			std::reverse(tCtrlPoint.begin(), tCtrlPoint.end());
			std::reverse(tCtrlWeight.begin(), tCtrlWeight.end());

			tCtrlPointNew = transpose<Vector3>(tCtrlPoint);
			tCtrlWeightNew = transpose<Scalar>(tCtrlWeight);

			// Update the coordinates of links
			foreach( Structure::Link * l, tg->getEdges(tSheet->id) ){
				std::vector<Vec4d> oldCoord = l->getCoord(tSheet->id), newCoord;
				foreach(Vec4d c, oldCoord) newCoord.push_back(Vec4d(c[1], 1- c[0], c[2], c[3]));
				l->setCoord(tSheet->id, newCoord);
			}
		}

		isModified = true;
		isUVFlipped = true;
	}

	// Create a new sheet if need
	if (isModified)
	{
		NURBSRectangle newRect; 
		if (isUVFlipped)
			newRect = NURBSRectangle(tCtrlPointNew, tCtrlWeightNew, vDegree, uDegree, vLoop, uLoop, vOpen, uOpen);
		else
			newRect = NURBSRectangle(tCtrlPointNew, tCtrlWeightNew, uDegree, vDegree, uLoop, vLoop, uOpen, vOpen);

		tSheet->surface = newRect;
	}
}

void GraphCorresponder::computeCorrespondences()
{
	// Part to Part correspondence
	computePartToPartCorrespondences();

	// Mark the nodes
	sIsCorresponded.resize(sg->nodes.size(), false);
	tIsCorresponded.resize(tg->nodes.size(), false);
	foreach (VECTOR_PAIR vector2vector, correspondences)
	{
		foreach (QString sID, vector2vector.first)
		{
			int sid = sg->getNode(sID)->property["index"].toInt();
			sIsCorresponded[sid] = true;
		}

		foreach (QString tID, vector2vector.second)
		{
			int tid = tg->getNode(tID)->property["index"].toInt();
			tIsCorresponded[tid] = true;
		}
	}

	// Adjust the frames for corresponded parts
	// Then Point-to-Point correspondences can be easily retrieved via parameterized NURBS. 
	for (int i = 0; i < (int)correspondences.size(); i++)
	{
		std::vector<QString> &sVector = correspondences[i].first;
		std::vector<QString> &tVector = correspondences[i].second;

		// One to many
		if (sVector.size() == 1)
		{
			Structure::Node *sNode = sg->getNode(*sVector.begin());
			foreach(QString tID, tVector)
			{
				Structure::Node *tNode = tg->getNode(tID);
				correspondTwoNodes(sNode, tNode);
			}
		}
		// Many to one
		else if (tVector.size() == 1)
		{
			Structure::Node *tNode = tg->getNode(*tVector.begin());
			foreach(QString sID, sVector)
			{
				Structure::Node *sNode = sg->getNode(sID);
				correspondTwoNodes(sNode, tNode);
			}
		}
		// Many to many
		else
		{
			qDebug() << "Many to Many correspondence happened. ";
		}
	}
}


void GraphCorresponder::saveLandmarks(QString filename)
{
	std::ofstream outF(filename.toStdString(), std::ios::out);

	outF << landmarks.size() << "\n\n";

	foreach (VECTOR_PAIR vector2vector, landmarks)
	{
		outF << vector2vector.first.size() << '\t';;
		foreach (QString strID, vector2vector.first)
			outF << strID.toStdString() << '\t';
		outF << '\n';

		outF << vector2vector.second.size() << '\t';
		foreach (QString strID, vector2vector.second)
			outF << strID.toStdString() << '\t';
		outF << "\n\n";
	}

	outF.close();
}

void GraphCorresponder::loadLandmarks(QString filename)
{
	std::ifstream inF(filename.toStdString(), std::ios::in);

	landmarks.clear();
	sIsLandmark.clear();
	tIsLandmark.clear();
	sIsLandmark.resize(sg->nodes.size(), false);
	tIsLandmark.resize(tg->nodes.size(), false);

	int nbCorr;
	inF >> nbCorr;

	for (int i = 0; i < nbCorr; i++)
	{
		int n;
		std::string strID;
		std::vector<QString> sParts, tParts;

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			sParts.push_back(QString(strID.c_str()));
		}

		inF >> n;
		for (int j = 0; j < n; j++)
		{
			inF >> strID;
			tParts.push_back(QString(strID.c_str()));
		}

		this->addLandmarks(sParts, tParts);
	}

	inF.close();
}

std::vector<QString> GraphCorresponder::nonCorresSource()
{
	std::vector<QString> nodes;
	
	for (int i = 0; i < (int)sIsCorresponded.size(); i++)
	{
		if (!sIsCorresponded[i])
			nodes.push_back(sg->nodes[i]->id);
	}

	return nodes;
}

std::vector<QString> GraphCorresponder::nonCorresTarget()
{
	std::vector<QString> nodes;

	for (int i = 0; i < (int) tIsCorresponded.size(); i++)
	{
		if (!tIsCorresponded[i])
			nodes.push_back(tg->nodes[i]->id);
	}

	return nodes;
}

QString GraphCorresponder::sgName()
{
	return sg->property["name"].toString().section('\\', -1).section('.', 0, 0);
}

QString GraphCorresponder::tgName()
{
	return tg->property["name"].toString().section('\\', -1).section('.', 0, 0);
}

void GraphCorresponder::saveCorrespondences( QString filename )
{

	std::ofstream outF("correspondences_one2one.txt", std::ios::out);

	outF << "Source shape: " << sg->property["name"].toString().toStdString() << '\n';
	outF << "Target shape: " << tg->property["name"].toString().toStdString() << "\n\n\n";
}

void GraphCorresponder::LoadCorrespondences( QString filename )
{

}