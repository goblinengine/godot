func union_param(x: int | String):
	print(x)

func test():
	var a := 1
	var b := 2
	swap(a, b)
	print(a)
	print(b)

	var arr_a := PackedByteArray([1, 2])
	var arr_b := PackedByteArray([9])
	swap(arr_a, arr_b)
	print(arr_a == PackedByteArray([9]))
	print(arr_b == PackedByteArray([1, 2]))

	var maybe: Dictionary | null = null
	print(maybe == null)
	maybe = {"k": "v"}
	print(maybe["k"])

	union_param(5)
	union_param("five")
