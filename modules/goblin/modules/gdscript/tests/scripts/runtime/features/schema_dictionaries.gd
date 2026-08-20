@schema const critter = { hp: int = 10, name: String = "", tags: Array[String] = [], sub: Dictionary = { level: int = 1 } }

var member_dict: Dictionary[critter]

func test():
	# Local without an initializer: defaults autofilled at construction.
	var m: Dictionary[critter]
	Utils.check(m.hp == 10)
	Utils.check(m.name == "")
	Utils.check(m.tags == [])
	Utils.check(m.sub.level == 1)

	# Typed-container defaults normalize at construction.
	Utils.check(m.tags.get_typed_builtin() == TYPE_STRING)

	# Override merge: literal wins, other keys keep defaults.
	var m2: Dictionary[critter] = { hp = 20 }
	Utils.check(m2.hp == 20)
	Utils.check(m2.name == "")

	# Empty literal -> defaults only.
	var e: Dictionary[critter] = {}
	Utils.check(e.hp == 10)

	# Growable beyond the schema; unknown keys are Variant.
	var m3: Dictionary[critter] = { hp = 30, loot = ["sword"] }
	Utils.check(m3.hp == 30)
	Utils.check(m3.loot == ["sword"])

	# Nested shaped dictionaries are mutable (instances own their defaults).
	var n: Dictionary[critter]
	n.sub.level = 7
	Utils.check(n.sub.level == 7)

	# Member variable without an initializer: implicit initializer fills defaults.
	Utils.check(member_dict.hp == 10)
	Utils.check(member_dict.name == "")

	# The schema constant itself is a plain (read-only) dictionary of defaults.
	Utils.check(critter.hp == 10)
	Utils.check(critter.get_typed_key_builtin() == TYPE_NIL)

	print('ok')
