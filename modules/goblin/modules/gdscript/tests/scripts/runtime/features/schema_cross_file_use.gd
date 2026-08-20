# Cross-file schema use: `crosscritter` is declared in schema_cross_file_def.gd and
# resolved through the global schema registry (the scan registered name -> script path).
func test():
	var c: Dictionary[crosscritter]
	Utils.check(c.hp == 5)
	Utils.check(c.dmg == 2)
	var c2: Dictionary[crosscritter] = { hp = 9 }
	Utils.check(c2.hp == 9)
	Utils.check(c2.dmg == 2)
	print('ok')
