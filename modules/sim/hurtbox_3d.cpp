/**************************************************************************/
/*  hurtbox_3d.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "hurtbox_3d.h"

#include "core/object/class_db.h"

void Hurtbox3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("apply_hit", "attacker", "hit_data"), &Hurtbox3D::apply_hit);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &Hurtbox3D::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &Hurtbox3D::is_active);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");

	ADD_SIGNAL(MethodInfo("hurt",
			PropertyInfo(Variant::OBJECT, "attacker", PROPERTY_HINT_RESOURCE_TYPE, "Object"),
			PropertyInfo(Variant::DICTIONARY, "hit_data")));
}

void Hurtbox3D::apply_hit(Object *p_attacker, const Dictionary &p_hit_data) {
	if (!active) {
		return;
	}
	emit_signal(SNAME("hurt"), p_attacker, p_hit_data);
}

void Hurtbox3D::set_active(bool p_active) {
	active = p_active;
}

bool Hurtbox3D::is_active() const {
	return active;
}

Hurtbox3D::Hurtbox3D() {
	// Passive receiver: no scanning, but detectable by active detectors.
	set_monitoring(false);
	set_monitorable(true);
}
