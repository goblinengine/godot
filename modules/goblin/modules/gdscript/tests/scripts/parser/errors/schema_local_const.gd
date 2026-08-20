func test():
	@schema
	const local_schema = { x: int = 1 }
	print('unreachable')
