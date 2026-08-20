func test() -> void:
	var dict = {
		a: int = 1,
		b: Array[int] = [1, 2, 3],
	}
	dict.a = "hi"
	dict.b = "not an array"
