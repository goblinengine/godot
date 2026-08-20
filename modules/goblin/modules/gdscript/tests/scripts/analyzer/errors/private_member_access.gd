const Helper := preload("private_access_helper.notest.gd")

func test() -> void:
	var helper: Helper.HelperClass = Helper.HelperClass.new()
	print(helper.private_var)
	print(helper.PRIVATE_CONST)
	helper.private_func()
	helper.public_func()