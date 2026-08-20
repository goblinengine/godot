class Outer:
	@private var secret: int = 42
	@private const SECRET_CONST: int = 7

	@private func secret_func() -> int:
		return secret

	@private class SecretInner:
		pass

	func read_all() -> int:
		return secret + SECRET_CONST + secret_func()

	func make_inner() -> SecretInner:
		return SecretInner.new()

class Nested:
	class Deep:
		func probe(o: Outer) -> int:
			return o.secret + o.SECRET_CONST + o.secret_func() + o.secret

		func touch_inner(o: Outer) -> Outer.SecretInner:
			return o.SecretInner.new()

func test() -> void:
	var outer: Outer = Outer.new()
	print(outer.read_all())
	var deep: Nested.Deep = Nested.Deep.new()
	print(deep.probe(outer))
	print(deep.touch_inner(outer) != null)