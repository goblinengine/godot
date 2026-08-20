@schema const mycritter = { hp: int = 10 }

func test():
	var bad = Dictionary[mycritter]
	print('unreachable')
