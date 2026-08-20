const Helper := preload("private_inner_helper.notest.gd")

func test() -> void:
	print(Helper.HelperOuter.PrivateInner)
	print(Helper.HelperOuter.PublicInner)