/**************************************************************************/
/*  visibility_component.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

// Same-directory include (file-relative): see entity_component.h.
#include "entity_component.h"

// Visibility/shadow flags for the entity's mesh instance: hide/show (stealth,
// cutscenes) and shadow control (low-fi perf management). Rides the mesh
// instance RID owned by the mesh pool; ~3 RenderingServer calls at flush.
class VisibilityComponent : public EntityComponent {
	GDCLASS(VisibilityComponent, EntityComponent);

public:
	void set_visible(bool p_visible);
	bool get_visible() const { return visible; }

	void set_cast_shadows(int32_t p_cast_shadows);
	int32_t get_cast_shadows() const { return cast_shadows; }

	ComponentType get_component_type() const override { return ComponentType::VISIBILITY; }

protected:
	void _attach() override;
	void _detach() override;
	static void _bind_methods();

private:
	bool visible = true;
	int32_t cast_shadows = 0; // RenderingServer::ShadowCastingSetting.
};
