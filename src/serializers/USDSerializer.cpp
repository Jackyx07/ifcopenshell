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

#ifdef WITH_USD

#include "USDSerializer.h"

#include "pxr/base/gf/vec3f.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"

USDSerializer::USDSerializer(const std::string& out_filename, const SerializerSettings& settings):
	WriteOnlyGeometrySerializer(settings),
	filename_(out_filename)
{
	// why does this not work with just the filename ???
	stage_ = pxr::UsdStage::CreateNew(filename_ + ".usda");

	if(!stage_)
		throw std::runtime_error("Could not create USD stage");

  	auto world = pxr::UsdGeomXform::Define(stage_, pxr::SdfPath("/World"));
  	stage_->SetDefaultPrim(world.GetPrim());
  	pxr::UsdGeomScope::Define(stage_, pxr::SdfPath("/Looks"));
  	createLighting();
  	ready_ = true;
}

USDSerializer::~USDSerializer() {
	
}

void USDSerializer::createLighting() {
  	const std::string& light_path = "/World/defaultLight";
  	pxr::UsdLuxDistantLight::Define(stage_, pxr::SdfPath(light_path));
  	pxr::UsdGeomXform xform(stage_->GetPrimAtPath(pxr::SdfPath(light_path)));
  	pxr::GfVec3f light_direction(0.0f, 0.0f, -1.0f);
  	pxr::UsdLuxDistantLight light(stage_->GetPrimAtPath(pxr::SdfPath(light_path)));
  	light.CreateIntensityAttr().Set(1000.0f);
  	light.CreateColorAttr().Set(pxr::GfVec3f(1.0f, 1.0f, 1.0f));
}

std::vector<pxr::UsdShadeMaterial> USDSerializer::createMaterials(const std::vector<IfcGeom::Material>& styles) 
{
  	if(styles.empty())
  	  	throw std::runtime_error("No styles to create materials from");

  	std::vector<pxr::UsdShadeMaterial> materials {};

  	for(auto style : styles) {
		std::string material_path(style.original_name());
		usd_utils::toPath(material_path);

		if(materials_.find(material_path) != materials_.end()) {
			materials.push_back(materials_[material_path]);
			continue;
		}

  	  	const std::string path("/Looks/" + material_path);
  	  	auto material = pxr::UsdShadeMaterial::Define(stage_, pxr::SdfPath(path));
  	  	auto shader = pxr::UsdShadeShader::Define(stage_, pxr::SdfPath(path + "/Shader"));
  	  	shader.CreateIdAttr().Set(pxr::TfToken("UsdPreviewSurface"));

  	  	float rgba[4] { 0.18f, 0.18f, 0.18f, 1.0f };
  	  	if (style.hasDiffuse())
			for (int i = 0; i < 3; ++i)
  	  	    	rgba[i] = static_cast<float>(style.diffuse()[i]);
  	  	shader.CreateInput(pxr::TfToken("diffuseColor"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(rgba[0], rgba[1], rgba[2]));

  	  	if(style.hasTransparency())
  	  		rgba[3] -= style.transparency();
  	  	shader.CreateInput(pxr::TfToken("opacity"), pxr::SdfValueTypeNames->Float).Set(rgba[3]);

  	  	if(style.hasSpecular()) {
  	  		for (int i = 0; i < 3; ++i)
  	  	    	rgba[i] = static_cast<float>(style.specular()[i]);
  	  	  	shader.CreateInput(pxr::TfToken("useSpecularWorkflow"), pxr::SdfValueTypeNames->Int).Set(1);
  	  	} else {
  	  	  	shader.CreateInput(pxr::TfToken("useSpecularWorkflow"), pxr::SdfValueTypeNames->Int).Set(0);
  	  	}
  	  	shader.CreateInput(pxr::TfToken("specularColor"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(rgba[0], rgba[1], rgba[2]));

  	  	material.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(), pxr::TfToken("surface"));
  	  	materials.push_back(material);
		materials_[material_path] = material;
  	}

  	return materials;
}

void USDSerializer::writeHeader() {
  	stage_->GetRootLayer()->SetComment("File generated by IfcOpenShell " + std::string(IFCOPENSHELL_VERSION)); 
}

void USDSerializer::write(const IfcGeom::TriangulationElement* o) {
  	const IfcGeom::Representation::Triangulation& mesh = o->geometry();
  	const auto verts = mesh.verts();
  	const auto faces = mesh.faces();
  	const auto material_ids = mesh.material_ids();
  	const std::vector<double>& m = o->transformation().matrix().data();

	if (material_ids.empty() || verts.empty() || faces.empty())
		return;

	std::string name = o->name();
  	if(name.empty()) {
  	  	name = "UnNamed";
  	} else {
  	  	name = usd_utils::toPath(name);
  	}
  	name += "_" + std::to_string(o->id());
  	auto usd_mesh = pxr::UsdGeomMesh::Define(stage_, pxr::SdfPath("/World/" + name));

  	usd_mesh.AddTranslateOp().Set(pxr::GfVec3d(m[9], m[10], m[11]));

  	pxr::VtVec3fArray points;
  	for(std::size_t i = 0; i < verts.size(); i+=3) {
  	  	points.push_back(pxr::GfVec3f(static_cast<float>(verts[i] * m[0] + verts[i+1] * m[3] + verts[i+2] * m[6]),
  	                                  static_cast<float>(verts[i] * m[1] + verts[i+1] * m[4] + verts[i+2] * m[7]),
  	                                  static_cast<float>(verts[i] * m[2] + verts[i+1] * m[5] + verts[i+2] * m[8])));
  	}
  	usd_mesh.CreatePointsAttr().Set(points);

  	usd_mesh.CreateFaceVertexIndicesAttr().Set(usd_utils::toVtArray(faces));
  	usd_mesh.CreateFaceVertexCountsAttr().Set(pxr::VtArray<int>((int) faces.size() / 3, 3));

  	pxr::VtVec3fArray normals;
  	for (std::vector<double>::const_iterator it = mesh.normals().begin(); it != mesh.normals().end();)
  	  	normals.push_back(pxr::GfVec3f(static_cast<float>(*(it++)), static_cast<float>(*(it++)), static_cast<float>(*(it++))));
  	usd_mesh.CreateNormalsAttr().Set(normals);

  	auto materials = createMaterials(mesh.materials());
  	pxr::UsdShadeMaterialBindingAPI material_api(usd_mesh);
  	if(materials.size() > 1) {
  	  	std::vector<pxr::VtArray<int>> subsets(materials.size());
  	  	for(int i = 0; i < material_ids.size(); ++i)
  	  	  	subsets[material_ids[i]].push_back(i);

  	  	for(std::size_t i = 0; i < subsets.size(); ++i){
  	  	  	auto subset = material_api.CreateMaterialBindSubset(pxr::TfToken("subset_" + std::to_string(i)), subsets[i]);
  	  	  	pxr::UsdShadeMaterialBindingAPI(subset).Bind(materials[i]);
  	  	}
  	} else {
  	  	material_api.Bind(materials[0]);
  	}
}

void USDSerializer::finalize() {
  	stage_->Save();
}

#endif // WITH_USD