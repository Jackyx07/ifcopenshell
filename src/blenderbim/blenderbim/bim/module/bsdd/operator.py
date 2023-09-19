# BlenderBIM Add-on - OpenBIM Blender Add-on
# Copyright (C) 2023 Dion Moult <dion@thinkmoult.com>
#
# This file is part of BlenderBIM Add-on.
#
# BlenderBIM Add-on is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# BlenderBIM Add-on is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with BlenderBIM Add-on.  If not, see <http://www.gnu.org/licenses/>.

import os
import bpy
import bsdd
import ifcopenshell
import blenderbim.tool as tool


class LoadBSDDDomains(bpy.types.Operator):
    bl_idname = "bim.load_bsdd_domains"
    bl_label = "Load bSDD Domains"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        props = context.scene.BIMBSDDProperties
        props.domains.clear()
        client = bsdd.Client()
        for domain in sorted(client.Domain(), key=lambda x: x["name"]):
            new = props.domains.add()
            new.name = domain["name"]
            new.namespace_uri = domain["namespaceUri"]
            new.default_language_code = domain["defaultLanguageCode"]
            new.organization_name_owner = domain["organizationNameOwner"]
            new.status = domain["status"]
            new.version = domain["version"]
        return {"FINISHED"}


class SetActiveBSDDDomain(bpy.types.Operator):
    bl_idname = "bim.set_active_bsdd_domain"
    bl_label = "Load bSDD Domains"
    bl_options = {"REGISTER", "UNDO"}
    name: bpy.props.StringProperty()
    uri: bpy.props.StringProperty()

    def execute(self, context):
        props = context.scene.BIMBSDDProperties
        props.active_domain = self.name
        props.active_uri = self.uri
        return {"FINISHED"}


class SearchBSDDClassifications(bpy.types.Operator):
    bl_idname = "bim.search_bsdd_classifications"
    bl_label = "Search bSDD Classifications"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        props = context.scene.BIMBSDDProperties
        props.classifications.clear()
        client = bsdd.Client()
        related_ifc_entities = []
        if len(props.keyword) < 3:
            return {"FINISHED"}
        if props.should_filter_ifc_class and context.active_object:
            element = tool.Ifc.get_entity(context.active_object)
            if element:
                related_ifc_entities = [element.is_a()]
        results = client.ClassificationSearchOpen(props.keyword, DomainNamespaceUris=[props.active_uri], RelatedIfcEntities=related_ifc_entities)
        for result in sorted(results["classifications"], key=lambda x: x["referenceCode"]):
            new = props.classifications.add()
            new.name = result["name"]
            new.reference_code = result["referenceCode"]
            new.description = result.get("description", "")
            new.namespace_uri = result["namespaceUri"]
            new.domain_name = result["domainName"]
            new.domain_namespace_uri = result["domainNamespaceUri"]
        return {"FINISHED"}