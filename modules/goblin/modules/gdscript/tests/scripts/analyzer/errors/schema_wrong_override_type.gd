@schema const mycritter = { hp: int = 10, name: String = "" }

func test():
	var bad: Dictionary[mycritter] = { hp = "hello" }
	print('unreachable')
