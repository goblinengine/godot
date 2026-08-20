func test():
	var a := {
		x: int = 1,
		pos: Vector3 = Vector3.ZERO,
		names: Array[String] = ["a", "b"],
		loot: Dictionary[String, int] = { "gold": 5 },
		mixed: int | String = "hi",
		w = 4,
		sub: Dictionary = {
			name: String = "hp",
			count: int = 3,
			deeper: Dictionary = {
				flag: bool = true,
			},
		},
	}

	# Shaped dictionaries are plain dictionaries at runtime.
	Utils.check(a.get_typed_key_builtin() == TYPE_NIL)
	Utils.check(a.get_typed_value_builtin() == TYPE_NIL)

	Utils.check(a.x == 1)
	Utils.check(a.pos == Vector3.ZERO)
	Utils.check(a.names == ["a", "b"])
	Utils.check(a.loot == { "gold": 5 })
	Utils.check(a.mixed == "hi")
	Utils.check(a.w == 4)
	Utils.check(a.sub.name == "hp")
	Utils.check(a.sub.count == 3)
	Utils.check(a.sub.deeper.flag == true)

	# Untyped keys are writable with any value, like a normal dictionary.
	a.w = "now a string"
	Utils.check(a.w == "now a string")
	a.new_key = 1.5
	Utils.check(a.new_key == 1.5)

	# Shaped dicts stay references, like plain dictionaries.
	var b := a
	b.x = 42
	Utils.check(a.x == 42)

	# Nested writes.
	a.sub.count = 9
	Utils.check(a.sub.count == 9)

	# A bare "Dictionary" annotation preserves the shape and value semantics.
	var d: Dictionary = { y: int = 2 }
	Utils.check(d.y == 2)
	d.y = 7
	Utils.check(d.y == 7)

	# A plain (untyped) declaration behaves the same at runtime.
	var plain = { p: int = 2, sub = { name: String = "mp" } }
	Utils.check(plain.p == 2)
	Utils.check(plain.sub.name == "mp")
	Utils.check(plain.get_typed_key_builtin() == TYPE_NIL)
	plain.other = "anything" # Unknown keys are Variant.
	Utils.check(plain.other == "anything")

	# A homogeneous typed annotation keeps the per-key shape while the runtime
	# dictionary is typed as declared.
	var typed_variant: Dictionary[StringName, Variant] = { t: int = 2 }
	Utils.check(typed_variant.t == 2)
	Utils.check(typed_variant.get_typed_key_builtin() == TYPE_STRING_NAME)
	Utils.check(typed_variant.get_typed_value_builtin() == TYPE_NIL)

	# Typed-container declarations keep the per-key shape AND the runtime container
	# typing: plain containers are normalized, the dictionary is typed as declared.
	# (Nested typed collections in declarations themselves are an upstream parser
	# limitation, so the deep entry is covered via a flat `Variant` declaration.)
	var plain_nums = [1, 2, 3] # Untyped array at runtime.
	var typed_deep: Dictionary[StringName, Variant] = { nums: Array[int] = plain_nums }
	Utils.check(typed_deep.nums == [1, 2, 3])
	Utils.check(typed_deep.nums.get_typed_builtin() == TYPE_INT)
	Utils.check(typed_deep.get_typed_key_builtin() == TYPE_STRING_NAME)
	Utils.check(typed_deep.get_typed_value_builtin() == TYPE_NIL)

	# Scalar typed keys are enforced by the flat dictionary types as well.
	var typed_scalar: Dictionary[String, int] = { "hp": int = 10, "mp": int = 3 }
	Utils.check(typed_scalar.hp == 10)
	Utils.check(typed_scalar.mp == 3)
	Utils.check(typed_scalar.get_typed_key_builtin() == TYPE_STRING)
	Utils.check(typed_scalar.get_typed_value_builtin() == TYPE_INT)

	print('ok')
