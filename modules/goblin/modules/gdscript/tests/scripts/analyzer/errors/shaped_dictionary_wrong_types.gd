func test() -> void:
	var a := {
		x: int = "not an int",
		y: String = 1,
		z: int | String = Vector2.ZERO,
		sub: Dictionary = { name: String = 5 },
	}
	var b := {
		x: int = 1,
	}
	b.x = "wrong"
	var c := {
		x: int = 1,
		sub = { name: String = "ok" },
	}
	c.sub.name = 3
	var d: Dictionary = { x: int = 1 }
	d.x = "wrong too"
