# Cross-file schema: this script declares `crosscritter`; the schema name is registered
# into the global registry during the test-suite scan, so other test scripts can use
# `Dictionary[crosscritter]` without any import.
@schema const crosscritter = { hp: int = 5, dmg: int = 2 }

func test():
	var c: Dictionary[crosscritter]
	Utils.check(c.hp == 5)
	Utils.check(c.dmg == 2)
	print('ok')
