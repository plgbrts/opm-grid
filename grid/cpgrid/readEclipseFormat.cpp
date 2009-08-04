//===========================================================================
//
// File: readEclipseFormat.cpp
//
// Created: Fri Jun 12 09:16:59 2009
//
// Author(s): Atgeirr F Rasmussen <atgeirr@sintef.no>
//            B�rd Skaflestad     <bard.skaflestad@sintef.no>
//
// $Date$
//
// $Revision$
//
//===========================================================================

/*
  Copyright 2009 SINTEF ICT, Applied Mathematics.
  Copyright 2009 Statoil ASA.

  This file is part of The Open Reservoir Simulator Project (OpenRS).

  OpenRS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OpenRS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OpenRS.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <fstream>
#include "../CpGrid.hpp"
#include "EclipseGridParser.hpp"
#include "EclipseGridInspector.hpp"
#include "../preprocess/preprocess.h"
#include "../common/GeometryHelpers.hpp"
#include "../common/StopWatch.hpp"

#define VERBOSE

namespace Dune
{


    // Forward declarations.
    namespace
    {
	void buildTopo(const processed_grid& output,
		       std::vector<int>& global_cell,
		       cpgrid::OrientedEntityTable<0, 1>& c2f,
		       cpgrid::OrientedEntityTable<1, 0>& f2c,
		       std::vector<array<int,8> >& c2p);
	void buildGeom(const processed_grid& output,
		       const cpgrid::OrientedEntityTable<0, 1>& c2f,
		       const std::vector<array<int,8> >& c2p,
		       cpgrid::DefaultGeometryPolicy& gpol,
		       cpgrid::SignedEntityVariable<FieldVector<double, 3> , 1>& normals,
		       std::vector<FieldVector<double, 3> >& allcorners);
	void logCartIndices(int idx, const processed_grid& output, int& i, int& j, int& k);
    } // anon namespace



    /// Read the Eclipse grid format ('.grdecl').
    void CpGrid::readEclipseFormat(const std::string& filename, double z_tolerance)
    {
	// Read eclipse file data.
#ifdef VERBOSE
	std::cout << "Parsing " << filename << std::endl;
#endif
	EclipseGridParser parser(filename);
	processEclipseFormat(parser, z_tolerance);
    }

    /// Read the Eclipse grid format ('.grdecl').
    void CpGrid::processEclipseFormat(const EclipseGridParser& parser, double z_tolerance)
    {
	EclipseGridInspector inspector(parser);

	// Make input struct for processing code.
	grdecl g;
	g.dims[0] = inspector.gridSize()[0];
	g.dims[1] = inspector.gridSize()[1];
	g.dims[2] = inspector.gridSize()[2];
	g.coord = &(parser.getFloatingPointValue("COORD")[0]);
	g.zcorn = &(parser.getFloatingPointValue("ZCORN")[0]);
	g.actnum = &(parser.getIntegerValue("ACTNUM")[0]);

	// Make the grid
	processEclipseFormat(g, z_tolerance);
    }

    /// Read the Eclipse grid format ('.grdecl').
    void CpGrid::processEclipseFormat(const grdecl& input_data, double z_tolerance)
    {
	// Process.
#ifdef VERBOSE
	std::cout << "Processing eclipse data." << std::endl;
#endif
	processed_grid output;
	process_grdecl(&input_data, z_tolerance, &output);

	/*
	//-------------------- Start compare code ----------------------------------
	const int num_cells = output.number_of_cells;   // Number of active cells
	const int num_faces = output.number_of_faces;
	const int num_points = output.number_of_nodes;
	std::cout << "Cells:" << num_cells << "   Faces:"  << num_faces
		  << "   Points:" << num_points << std::endl;


// 	// Faces cell numbers
// 	std::cout << "\nFace number, internal cell numbers" << std::endl;
// 	for (int i=0; i<output.number_of_faces; ++i){
// 	    int c1 = output.face_neighbors[2*i];
// 	    int c2 = output.face_neighbors[2*i+1];
// 	    std::cout << i << "  " << c1 << "  " << c2 << std::endl;
// 	}

	std::map<int,int> global_local;
	for (int i=0; i<num_cells; ++i) {
	    int gi = output.local_cell_index[i];
	    //	    std::cout << i << " i - gi " << gi << std::endl;
	    global_local.insert(std::make_pair(gi,i));
	}
	std::cout << "\nLogical cartesian cell-number,  i, j, k"
		  << std::endl;
	for (std::map<int,int>::iterator pos = global_local.begin();
	     pos != global_local.end(); ++pos) {
	    const int gi = pos->first;
	    int i, j, k;
	    logCartIndices(gi, output, i, j, k);
	    std::cout << gi << "  " << i << " " << j << " " << k
		      << std::endl;
	}
	//------------------- End compare code ----------------------------------
	*/

	// Move data into the grid's structures.
#ifdef VERBOSE
	std::cout << "Building topology." << std::endl;
#endif
	buildTopo(output, global_cell_, cell_to_face_, face_to_cell_, cell_to_point_);

#ifdef VERBOSE
	std::cout << "Building geometry." << std::endl;
#endif
	buildGeom(output, cell_to_face_, cell_to_point_, geometry_, face_normals_, allcorners_);

#ifdef VERBOSE
        std::cout << "Assigning face tags." << std::endl;
#endif
        face_tag_.assign(output.face_tag,
                         output.face_tag + output.number_of_faces);

#ifdef VERBOSE
	std::cout << "Cleaning up." << std::endl;
#endif
	// Clean up the output struct.
	free_processed_grid(&output);
#ifdef VERBOSE
	std::cout << "Done with grid processing." << std::endl;
#endif
    }


    namespace
    {

	void buildTopo(const processed_grid& output,
		       std::vector<int>& global_cell,
		       cpgrid::OrientedEntityTable<0, 1>& c2f,
		       cpgrid::OrientedEntityTable<1, 0>& f2c,
		       std::vector<array<int,8> >& c2p)
	{
	    // Map local to global cell index.
	    global_cell.assign(output.local_cell_index,
			       output.local_cell_index + output.number_of_cells);

	    // Build face to cell.
	    f2c.clear();
	    int nf = output.number_of_faces;
	    cpgrid::EntityRep<0> cells[2];
	    for (int i = 0; i < nf; ++i) {
		const int* fnc = output.face_neighbors + 2*i;
		int cellcount = 0;
		if (fnc[0] != -1) {
		    cells[cellcount].setValue(fnc[0], true);
		    ++cellcount;
		}
		if (fnc[1] != -1) {
		    cells[cellcount].setValue(fnc[1], false);
		    ++cellcount;
		}
		ASSERT(cellcount == 1 || cellcount == 2);
		f2c.appendRow(cells, cells + cellcount);
	    }

	    // Build cell to face.
	    f2c.makeInverseRelation(c2f);

	    // Build cell to point
// 	    const cpgrid::EntityRep<3>* dummy = 0;
// 	    for (int i = 0; i < c2f.size(); ++i) {
// 		c2p.appendRow(dummy, dummy);
// 	    }
	    c2p.clear();
	    c2p.reserve(c2f.size());
	    for (int i = 0; i < c2f.size(); ++i) {
		cpgrid::OrientedEntityTable<0, 1>::row_type cf = c2f[cpgrid::EntityRep<0>(i)];
		// We know that the bottom and top faces come last.
		int numf = cf.size();
		int bot_face = cf[numf - 2].index();
		int bfbegin = output.face_ptr[bot_face];
		ASSERT(output.face_ptr[bot_face + 1] - bfbegin == 4);
		int top_face = cf[numf - 1].index();
		int tfbegin = output.face_ptr[top_face];
		ASSERT(output.face_ptr[top_face + 1] - tfbegin == 4);
		// We want the corners in 'x fastest, then y, then z' order,
		// so we need to take the face_nodes in noncyclic order: 0 1 3 2.
		array<int,8> corners = {{ output.face_nodes[bfbegin],
					  output.face_nodes[bfbegin + 1],
					  output.face_nodes[bfbegin + 3],
					  output.face_nodes[bfbegin + 2],
					  output.face_nodes[tfbegin],
					  output.face_nodes[tfbegin + 1],
					  output.face_nodes[tfbegin + 3],
					  output.face_nodes[tfbegin + 2] }};
		c2p.push_back(corners);
	    }
#ifndef NDEBUG
#ifdef VERBOSE
	    std::cout << "Doing extra topology integrity check." << std::endl;
#endif
	    cpgrid::OrientedEntityTable<1, 0> f2c_again;
	    c2f.makeInverseRelation(f2c_again);
	    ASSERT(f2c == f2c_again);
#endif
	}


	template <typename T>
	class IndirectArray
	{
	public:
	    IndirectArray(const std::vector<T>& data, const int* beg, const int* end)
		: data_(data), beg_(beg), end_(end)
	    {
	    }
	    const T& operator[](int index) const
	    {
		ASSERT(index >= 0 && index < size());
		return data_[beg_[index]];
	    }
	    int size() const
	    {
		return end_ - beg_;
	    }
	    typedef T value_type;
	private:
	    const std::vector<T>& data_;
	    const int* beg_;
	    const int* end_;
	};

	template <int dim>
	struct MakeGeometry
	{
	    cpgrid::Geometry<dim, 3> operator()(const FieldVector<double, 3>& pos, double vol = 1.0)
	    {
		return cpgrid::Geometry<dim, 3>(pos, vol);
	    }
	};

	template <>
	struct MakeGeometry<3>
	{
	    const FieldVector<double, 3>* allcorners_;
	    MakeGeometry(const FieldVector<double, 3>* allcorners)
		: allcorners_(allcorners)
	    {
	    }
	    cpgrid::Geometry<3, 3> operator()(const FieldVector<double, 3>& pos,
					      double vol,
					      const array<int,8>& corner_indices)
	    {
		return cpgrid::Geometry<3, 3>(pos, vol, allcorners_, &corner_indices[0]);
	    }
	};


	void buildGeom(const processed_grid& output,
		       const cpgrid::OrientedEntityTable<0, 1>& c2f,
		       const std::vector<array<int,8> >& c2p,
		       cpgrid::DefaultGeometryPolicy& gpol,
		       cpgrid::SignedEntityVariable<FieldVector<double, 3>, 1>& normals,
		       std::vector<FieldVector<double, 3> >& allcorners)
	{
	    typedef FieldVector<double, 3> point_t;
	    std::vector<point_t>& points = allcorners;
	    std::vector<point_t> face_normals;
	    std::vector<point_t> face_centroids;
	    std::vector<double>  face_areas;
	    std::vector<point_t> cell_centroids;
	    std::vector<double>  cell_volumes;
	    using namespace GeometryHelpers;
#ifdef VERBOSE
	    time::StopWatch clock;
	    clock.start();
#endif
	    // Get the points.
	    int np = output.number_of_nodes;
	    points.clear();
	    points.reserve(np);
	    for (int i = 0; i < np; ++i) {
		// \TODO add a convenience explicit constructor
		// for FieldVector taking an iterator.
		point_t pt;
		for (int dd = 0; dd < 3; ++dd) {
		    pt[dd] = output.node_coordinates[3*i + dd];
		}
		points.push_back(pt);
	    }
#ifdef VERBOSE
	    std::cout << "Points:             " << clock.secsSinceLast() << std::endl;
#endif

	    // Get the face data.
	    // \TODO Both the face and (especially) the cell section
	    // is not very efficient. It could be rewritten easily
	    // (focus on the polygonCellXXX methods first).
	    // \TODO Use exact geometry instead of these approximations.
	    int nf = output.number_of_faces;
	    const int* fn = output.face_nodes;
	    const int* fp = output.face_ptr;
	    for (int face = 0; face < nf; ++face) {
		// Computations in this loop could be speeded up
		// by doing more of them simultaneously.
		IndirectArray<point_t> face_pts(points, fn + fp[face], fn + fp[face+1]);
		point_t avg = average(face_pts);
		point_t centroid = polygonCentroid(face_pts, avg);
		point_t normal = polygonNormal(face_pts, centroid);
		double area = polygonArea(face_pts, centroid);
		face_normals.push_back(normal);
		face_centroids.push_back(centroid);
		face_areas.push_back(area);
	    }
#ifdef VERBOSE
	    std::cout << "Faces:              " << clock.secsSinceLast() << std::endl;
#endif
	    // Get the cell data.
	    int nc = output.number_of_cells;
	    std::vector<int> face_indices;
	    for (int cell = 0; cell < nc; ++cell) {
		cpgrid::EntityRep<0> cell_ent(cell, true);
		cpgrid::OrientedEntityTable<0, 1>::row_type cf = c2f[cell_ent];
		face_indices.clear();
		for (int local_index = 0; local_index < cf.size(); ++local_index) {
		    face_indices.push_back(cf[local_index].index());
		}
		IndirectArray<point_t> cell_pts(face_centroids, &face_indices[0], &face_indices[0] + cf.size());
		point_t cell_avg = average(cell_pts);
		point_t cell_centroid(0.0);
		double tot_cell_vol = 0.0;
		for (int local_index = 0; local_index < cf.size(); ++local_index) {
		    int face = cf[local_index].index();
		    IndirectArray<point_t> face_pts(points, fn + fp[face], fn + fp[face+1]);
		    double small_vol = polygonCellVolume(face_pts, face_centroids[face], cell_avg);
		    tot_cell_vol += small_vol;
		    point_t face_contrib = polygonCellCentroid(face_pts, face_centroids[face], cell_avg);
		    face_contrib *= small_vol;
		    cell_centroid += face_contrib;
		}
		cell_centroid /= tot_cell_vol;
		cell_centroids.push_back(cell_centroid);
		cell_volumes.push_back(tot_cell_vol);
	    }
#ifdef VERBOSE
	    std::cout << "Cells:              " << clock.secsSinceLast() << std::endl;
#endif


	    // \TODO Fix the code below , as it is:
	    // A) wasteful
	    // B) wordy,
	    // C) slow
	    // D) copied from readSintefLegacyFormat.cpp
	    // Cells
	    cpgrid::EntityVariable<cpgrid::Geometry<3, 3>, 0> cellgeom;
	    std::vector<cpgrid::Geometry<3, 3> > cg;
	    cg.reserve(nc);
	    MakeGeometry<3> mcellg(&allcorners[0]);
// 	    std::transform(cell_centroids.begin(), cell_centroids.end(),
// 			   cell_volumes.begin(),
// 			   std::back_inserter(cg), mcellg);
	    for (int c = 0;  c < nc; ++c) {
		cg.push_back(mcellg(cell_centroids[c], cell_volumes[c], c2p[c]));
	    }
	    cellgeom.assign(cg.begin(), cg.end());
	    // Faces
	    cpgrid::EntityVariable<cpgrid::Geometry<2, 3>, 1> facegeom;
	    std::vector<cpgrid::Geometry<2, 3> > fg;
	    MakeGeometry<2> mfaceg;
	    std::transform(face_centroids.begin(), face_centroids.end(),
			   face_areas.begin(),
			   std::back_inserter(fg), mfaceg);
	    facegeom.assign(fg.begin(), fg.end());
	    // Points
	    cpgrid::EntityVariable<cpgrid::Geometry<0, 3>, 3> pointgeom;
	    std::vector<cpgrid::Geometry<0, 3> > pg;
	    MakeGeometry<0> mpointg;
	    std::transform(points.begin(), points.end(),
			   std::back_inserter(pg), mpointg);
	    pointgeom.assign(pg.begin(), pg.end());
#ifdef VERBOSE
	    std::cout << "Transforms/copies:  " << clock.secsSinceLast() << std::endl;
#endif

	    // The final, combined object (yes, a lot of copying goes on here).
	    cpgrid::DefaultGeometryPolicy gp(cellgeom, facegeom, pointgeom);
	    gpol = gp;
	    normals.assign(face_normals.begin(), face_normals.end());
#ifdef VERBOSE
	    std::cout << "Final construction: " << clock.secsSinceLast() << std::endl;
#endif
	}

	void logCartIndices(int idx, const processed_grid& output, int& i, int& j, int& k)
	{
	    /// Computes logical cartesian indices i, j and k from linear index idx.
	    const int Nx = output.dimensions[0];
	    const int Ny = output.dimensions[1];
	    const int NxNy = Nx*Ny;
	    k = idx/NxNy;
	    j = (idx - NxNy*k)/Nx;
	    i = idx - Nx*j - Nx*Ny*k;
	}

    } // anon namespace




} // namespace Dune

