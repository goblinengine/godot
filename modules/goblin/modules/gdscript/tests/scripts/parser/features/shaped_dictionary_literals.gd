func test():
	var _a := {
		x: int = 1,
		pos: Vector3 = Vector3.ZERO,
		names: Array[String] = ["a", "b"],
		loot: Dictionary[String, int] = { "gold": 5 },
		mixed: int | String = "hi",
		w = 4,
		sub = { name: String = "hp" },
		sub_typed: Dictionary = {
			name: String = "mp",
			count: int = 3,
			deeper: Dictionary = {
				flag: bool = true,
			},
		},
	}
	print("ok")
