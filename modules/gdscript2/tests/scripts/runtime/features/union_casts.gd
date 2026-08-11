func test():
	var x : int | float = 2.5
	var y : int | float = 2
	var a : int | String = "hello"

	print(x as int + y as int) # 4
	print(a as int + 1) # 1 because "hello" -> 0
	print(y as String + "1") # "21"
	print(a as String + "1") # "hello1"

	var name : int = 1
	print(name as String) # "1"
	var val1 : int = 2
	print(val1 as String + "2") # "22"
