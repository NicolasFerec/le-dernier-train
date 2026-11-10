class_name Isometric
extends RefCounted

const GROUND := Transform2D(Vector2(0.866, 0.5), Vector2(-0.866, 0.5), Vector2.ZERO)


static func depth(projected_y: float) -> int:
	return 3 + maxi(0, roundi(projected_y))
