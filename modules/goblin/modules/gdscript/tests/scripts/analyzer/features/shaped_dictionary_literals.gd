func test():
	var a := {
		x: int = 1,
		pos: Vector3 = Vector3.ZERO,
		names: Array[String] = ["a", "b"],
		loot: Dictionary[String, int] = { "gold": 5 },
		sub: Dictionary = {
			name: String = "hp",
			count: int = 3,
			deeper: Dictionary = {
				flag: bool = true,
			},
		},
		w = 4,
		untyped_sub = { name: String = "mp" },
	}

	# Shaped access refines to the entry type at any depth.
	var as_int: int = a.x
	var as_vec3: Vector3 = a.pos
	var as_array: Array[String] = a.names
	var as_dict: Dictionary[String, int] = a.loot
	var as_sub_name: String = a.sub.name
	var as_sub_count: int = a.sub.count
	var as_deeper_flag: bool = a.sub.deeper.flag
	var as_index_name: String = a["sub"]["name"]
	var as_untyped_nested: String = a.untyped_sub.name

	Utils.check(as_int == 1)
	Utils.check(as_vec3 == Vector3.ZERO)
	Utils.check(as_array == ["a", "b"])
	Utils.check(as_dict == { "gold": 5 })
	Utils.check(as_sub_name == "hp")
	Utils.check(as_sub_count == 3)
	Utils.check(as_deeper_flag == true)
	Utils.check(as_index_name == "hp")
	Utils.check(as_untyped_nested == "mp")

	# A bare "Dictionary" annotation preserves the shape.
	var d: Dictionary = { y: int = 2 }
	var dy: int = d.y
	Utils.check(dy == 2)

	# A plain (untyped) declaration keeps the shape for access too.
	var untyped_declared = { ua: int = 3 }
	var ua: int = untyped_declared.ua
	Utils.check(ua == 3)

	# A homogeneous typed annotation keeps the per-key shape as well.
	var typed_variant: Dictionary[StringName, Variant] = { ta: int = 4 }
	var ta: int = typed_variant.ta
	Utils.check(ta == 4)
	typed_variant.other = "anything" # Unknown keys stay Variant.
	Utils.check(typed_variant.other == "anything")

	# Typed-container declarations refine per-key like the other styles.
	var tdeep: Dictionary[StringName, Variant] = { nums: Array[int] = [1, 2] }
	var tnums: Array[int] = tdeep.nums
	Utils.check(tnums == [1, 2])

	var tscalar: Dictionary[String, int] = { "hp": int = 7 }
	var thp: int = tscalar.hp
	Utils.check(thp == 7)

	# Unknown keys fall back to Variant.
	var unknown: Variant = a.get("not_there")
	Utils.check(unknown == null)

	print('ok')
