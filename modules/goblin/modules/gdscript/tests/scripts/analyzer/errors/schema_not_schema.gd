const plain_const = { a = 1 }

func test():
	var bad: Dictionary[plain_const]
	print('unreachable')
