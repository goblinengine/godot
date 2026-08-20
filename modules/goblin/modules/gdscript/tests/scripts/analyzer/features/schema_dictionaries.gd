@schema const mycritter = { hp: int = 10, name: String = "", pos: Vector2 = Vector2.ZERO, tags: Array[String] = [] }
@schema const npc = { title: String = "guard", level: int = 1 }

func test():
	# Access refinement: reads through the schema shape are statically typed.
	var m: Dictionary[mycritter]
	var hp: int = m.hp
	var name: String = m.name
	var pos: Vector2 = m.pos
	var tags: Array[String] = m.tags
	var as_hp: int = m["hp"]
	var as_name: String = m["name"]

	# Override-merge literals validate against the schema shape.
	var m2: Dictionary[mycritter] = { hp = 20 }
	var m2_hp: int = m2.hp

	# Unknown keys fall back to Variant (growable).
	var m3: Dictionary[mycritter] = { hp = 30, loot = ["sword"] }
	var loot: Variant = m3.loot

	# Function parameters can be schema-typed.
	var r: int = consume(m2)
	Utils.check(r == 20)

	# Another schema in the same script.
	var g: Dictionary[npc] = { title = "boss" }
	var g_title: String = g.title
	Utils.check(g_title == "boss")

	Utils.check(hp == 10)
	Utils.check(name == "")
	Utils.check(pos == Vector2.ZERO)
	Utils.check(tags == [])
	Utils.check(as_hp == 10)
	Utils.check(as_name == "")
	Utils.check(m2_hp == 20)
	Utils.check(loot == ["sword"])

	print('ok')

func consume(p: Dictionary[mycritter]) -> int:
	return p.hp
