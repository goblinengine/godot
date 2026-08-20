func test():
	var bad: Variant = "wrong"
	var a = { x: int = bad }
	print(a.x)
