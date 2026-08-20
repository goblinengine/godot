extends Node

# EntityNode/EntityComponent smoke test (D-20).
#
# Headless verification of the full pipeline from GDScript: build an entity
# with all three batch-1 components, compile-on-enter, mutate via property
# setters, run one flush (frame boundary), then quit with the marker line.
#
# Run (scratch project): bin/goblin.windows.editor.x86_64.exe --headless --path <scratch>
# Expect: ENTITY_SMOKE_OK + no script errors.

var _failed := false


func _check(p_cond: bool, p_what: String) -> void:
	if not p_cond:
		push_error("ENTITY_SMOKE FAIL: " + p_what)
		_failed = true


func _ready() -> void:
	var entity := EntityNode.new()
	var transform := Transform3DComponent.new()
	transform.transform = Transform3D(Basis.IDENTITY, Vector3(1, 2, 3))
	var mesh := MeshInstanceComponent.new()
	mesh.mesh = BoxMesh.new()
	var visibility := VisibilityComponent.new()
	entity.add_child(transform)
	entity.add_child(mesh)
	entity.add_child(visibility)
	add_child(entity)

	_check(entity.get_entity_id() != 0, "entity_id assigned")
	_check(entity.has_component(EntityNode.ComponentType.TRANSFORM3D), "has transform component")
	_check(entity.has_component(EntityNode.ComponentType.MESH_INSTANCE), "has mesh component")
	_check(entity.has_component(EntityNode.ComponentType.VISIBILITY), "has visibility component")
	_check(entity.get_component(EntityNode.ComponentType.TRANSFORM3D) == transform, "get_component resolves")
	_check(entity.get_type_mask() != 0, "type mask populated")

	# Mutate via setters; the pool mirror + dirty flag are driven by these.
	transform.transform = Transform3D(Basis.IDENTITY, Vector3(4, 5, 6))
	visibility.visible = false
	visibility.cast_shadows = 1

	# Run one flush at the frame boundary (creates the mesh instance RID on the
	# server; the dummy/headless renderer discards it, so no-crash is the check).
	await get_tree().process_frame

	_check(transform.transform.origin == Vector3(4, 5, 6), "transform node state kept")
	_check(visibility.visible == false, "visibility node state kept")

	remove_child(entity)
	entity.free()

	if _failed:
		get_tree().quit(1)
		return
	print("ENTITY_SMOKE_OK")
	get_tree().quit()
