func test() -> void:
	var dict: Dictionary[StringName, Variant] = {
		a: int = 1,
		b: Array[int] = [1, 2, 3],
	}
	dict.a = "hi"
	dict["b"] = 1.5
