/********************************************************************************
 *                                                                              *
 * This file is part of IfcOpenShell.                                           *
 *                                                                              *
 * IfcOpenShell is free software: you can redistribute it and/or modify         *
 * it under the terms of the Lesser GNU General Public License as published by  *
 * the Free Software Foundation, either version 3.0 of the License, or          *
 * (at your option) any later version.                                          *
 *                                                                              *
 * IfcOpenShell is distributed in the hope that it will be useful,              *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of               *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                 *
 * Lesser GNU General Public License for more details.                          *
 *                                                                              *
 * You should have received a copy of the Lesser GNU General Public License     *
 * along with this program. If not, see <http://www.gnu.org/licenses/>.         *
 *                                                                              *
 ********************************************************************************/

#include "OpenCascadeKernel.h"

#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepAlgoAPI_Common.hxx>

using namespace ifcopenshell::geometry;
using namespace ifcopenshell::geometry::kernels;
using namespace IfcGeom;
using namespace IfcGeom::util;

bool OpenCascadeKernel::convert(const taxonomy::solid* solid, TopoDS_Shape& result) {
	BRep_Builder BB;
	TopoDS_Solid S;
	for (auto& s : solid->children) {
		if (s->kind() == taxonomy::FACE) {
			// halfspace
			if (solid->children.size() != 1) {
				throw std::runtime_error("Unexpected number of children on solid");
			}

			auto face = (taxonomy::face*) s;

			const auto& m = ((taxonomy::geom_item*)face->basis)->matrix.ccomponents();
			gp_Pln pln(convert_xyz2<gp_Pnt>(m.col(3)), convert_xyz2<gp_Dir>(m.col(2)));
			const gp_Pnt pnt = pln.Location().Translated(face->orientation.get_value_or(false) ? pln.Axis().Direction() : -pln.Axis().Direction());
			TopoDS_Shape halfspace = BRepPrimAPI_MakeHalfSpace(BRepBuilderAPI_MakeFace(pln), pnt).Solid();

			if (!face->children.empty()) {
				TopoDS_Wire wire;
				gp_GTrsf gtrsf;

				if (convert((taxonomy::loop*)face->children[0], wire) && wire.Closed() && convert(&face->matrix, gtrsf)) {
					gp_Trsf trsf = gtrsf.Trsf();
					TopoDS_Shape prism = BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire), gp_Vec(0, 0, 200));
					gp_Trsf down; down.SetTranslation(gp_Vec(0, 0, -100.0));

					// `trsf` and `down` both have a unit scale factor
					prism.Move(trsf*down);

					halfspace = BRepAlgoAPI_Common(halfspace, prism);
				}
			}

			result = halfspace;
			return true;
		} else {
			if (s->kind() != taxonomy::SHELL) {
				throw std::runtime_error("Unexpected child in solid");
			}
			if (S.IsNull()) {
				BB.MakeSolid(S);
			}
			TopoDS_Shell shl;
			convert(((taxonomy::shell*)s), shl);
			BB.Add(S, shl);
		}
	}
	if (!S.IsNull()) {
		result = S;
	}
}

bool OpenCascadeKernel::convert_impl(const taxonomy::solid* solid , IfcGeom::ConversionResults& results) {
	TopoDS_Shape shape;
	if (!convert(solid, shape)) {
		return false;
	}
	results.emplace_back(ConversionResult(
		solid->instance->data().id(),
		solid->matrix,
		new OpenCascadeShape(shape),
		solid->surface_style
	));
	return true;
}