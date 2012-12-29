#pragma once

#include <QObject>
#include "DynamicGraph.h"
#include "GraphDistance.h"

namespace Structure{
	typedef QPair<Node*,Node*> QPairNodes;
	typedef QPair<Link*,Link*> QPairLink;
	typedef QPair<Scalar, QPairLink> ScalarLinksPair;

	struct Graph;

	class TopoBlender : public QObject
	{
		Q_OBJECT
	public:
		explicit TopoBlender(Graph * graph1, Graph * graph2, QObject *parent = 0);

		Graph * g1;
		Graph * g2;

		GraphDistance * originalGraphDistance;

		DynamicGraph source;
		DynamicGraph active;
		DynamicGraph target;

		QMap<QString, QVariant> params;

		Graph * blend();
		void materializeInBetween( Graph * graph, double t, Graph * sourceGraph );

		QList< ScalarLinksPair > badCorrespondence( QString activeNodeID, QString targetNodeID, 
			QMap< Link*, std::vector<Vec4d> > & coord_active, QMap< Link*, std::vector<Vec4d> > & coord_target );

		// Logging
		int stepCounter;
		void visualizeActiveGraph(QString caption, QString subcaption);

		// Preprocessing
		void cleanup();

	public slots:
		void bestPartialCorrespondence();

	public:
		// DEBUG:
		std::vector< Vector3 > debugPoints;
		std::vector< PairVector3 > debugLines;
		void drawDebug();

	signals:

	};
}