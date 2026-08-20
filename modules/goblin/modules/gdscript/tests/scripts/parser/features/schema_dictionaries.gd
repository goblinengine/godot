@schema const mycritter = { hp: int = 10, name: String = "", pos: Vector2 = Vector2.ZERO }

func test():
	# `@schema const` (typed entries + defaults) and `Dictionary[Name]` instantiation parse.
	var m: Dictionary[mycritter]
	var m2: Dictionary[mycritter] = { hp = 20 }
	print('ok', m.hp + m2.hp)
