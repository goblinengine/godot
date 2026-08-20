/**************************************************************************/
/*  transform_3d_component.h                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

// Same-directory include (file-relative): see entity_component.h.
#include "entity_component.h"

// Entity-local transform component. The flush composes it with the entity's
// world anchor (nearest Node3D ancestor): an EntityNode under a moving Node3D
// inherits the Node3D's global transform.
class Transform3DComponent : public EntityComponent {
	GDCLASS(Transform3DComponent, EntityComponent);

public:
	void set_transform(const Transform3D &p_transform);
	Transform3D get_transform() const { return transform; }

	ComponentType get_component_type() const override { return ComponentType::TRANSFORM3D; }

protected:
	void _attach() override;
	void _detach() override;
	static void _bind_methods();

private:
	Transform3D transform;
};
