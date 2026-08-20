/**************************************************************************/
/*  mesh_instance_component.h                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

// Same-directory include (file-relative): see entity_component.h.
#include "entity_component.h"

#include "core/variant/array.h"

// Mesh + per-surface material component. The pool owns the RenderingServer
// instance RID (created at flush, freed on detach); mesh/material writes
// mark the slot dirty for the batched flush. Node properties are
// authoritative; the pool is the compiled mirror.
class MeshInstanceComponent : public EntityComponent {
	GDCLASS(MeshInstanceComponent, EntityComponent);

public:
	void set_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_mesh() const { return mesh; }

	void set_material_overrides(const Array &p_materials); // Array[Ref<Material>], per surface.
	Array get_material_overrides() const { return material_overrides; }

	ComponentType get_component_type() const override { return ComponentType::MESH_INSTANCE; }

protected:
	void _attach() override;
	void _detach() override;
	static void _bind_methods();

private:
	Ref<Mesh> mesh;
	Array material_overrides;
};
