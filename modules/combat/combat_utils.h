/**************************************************************************/
/*  combat_utils.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

// Shared hit-data contract for the combat module.
//
// Every damage event in the module travels as a Dictionary with a stable key
// set, so Hitbox3D, Hurtbox3D, and Projectile3D interoperate and game code
// can consume any of them with the same accessors.
//
// Keys:
//   "damage"        float          raw damage amount
//   "knockback"     float          knockback force
//   "damage_types"  Array[StringName]  damage type tags (fire, cold, ...)
//   "element"       StringName     elemental flavor tag ("" = none)
//   "source"        Object         the attacker (Hitbox3D, Projectile3D, ...)
//   "position"      Vector3        world-space impact point (may be zero)
//   "normal"        Vector3        world-space impact normal (may be zero)
//   "velocity"      Vector3        attacker velocity at impact (may be zero)
//   "collider"      Object         object that was hit (may be null)

namespace CombatUtils {

constexpr const char *KEY_DAMAGE = "damage";
constexpr const char *KEY_KNOCKBACK = "knockback";
constexpr const char *KEY_DAMAGE_TYPES = "damage_types";
constexpr const char *KEY_ELEMENT = "element";
constexpr const char *KEY_SOURCE = "source";
constexpr const char *KEY_POSITION = "position";
constexpr const char *KEY_NORMAL = "normal";
constexpr const char *KEY_VELOCITY = "velocity";
constexpr const char *KEY_COLLIDER = "collider";

// Builds a hit-data dictionary from the standard fields. All keys are always
// present so consumers can read them without `has()` checks.
inline Dictionary build_hit_data(float p_damage, float p_knockback, const Array &p_damage_types,
		const StringName &p_element, Object *p_source, const Vector3 &p_position,
		const Vector3 &p_normal, Object *p_collider, const Vector3 &p_velocity) {
	Dictionary data;
	data[KEY_DAMAGE] = p_damage;
	data[KEY_KNOCKBACK] = p_knockback;
	data[KEY_DAMAGE_TYPES] = p_damage_types;
	data[KEY_ELEMENT] = p_element;
	data[KEY_SOURCE] = p_source;
	data[KEY_POSITION] = p_position;
	data[KEY_NORMAL] = p_normal;
	data[KEY_COLLIDER] = p_collider;
	data[KEY_VELOCITY] = p_velocity;
	return data;
}

} // namespace CombatUtils
