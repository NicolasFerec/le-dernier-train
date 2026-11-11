extends RefCounted

# Ground-relative grip offsets in atlas order (S, SW, W, NW, N, NE, E, SE).
const OFFSETS := {
	0: [
		[Vector2(-12, -32)],
		[Vector2(-10, -34)],
		[Vector2(-1, -28)],
		[Vector2(9, -35)],
		[Vector2(12, -33)],
		[Vector2(8, -35)],
		[Vector2(-3, -29)],
		[Vector2(-11, -30)],
	],
	1: [
		[Vector2(-8, -36), Vector2(-7, -36), Vector2(-8, -33), Vector2(-12, -34)],
		[Vector2(-12, -39), Vector2(-10, -37), Vector2(-16, -34), Vector2(-10, -35)],
		[Vector2(-19, -37), Vector2(-15, -36), Vector2(-18, -30), Vector2(-18, -37)],
		[Vector2(-18, -37), Vector2(-13, -37), Vector2(-18, -31), Vector2(-17, -36)],
		[Vector2(12, -35), Vector2(13, -35), Vector2(12, -32), Vector2(12, -33)],
		[Vector2(13, -38), Vector2(13, -41), Vector2(13, -33), Vector2(13, -41)],
		[Vector2(14, -39), Vector2(13, -36), Vector2(10, -31), Vector2(15, -39)],
		[Vector2(9, -32), Vector2(-3, -32), Vector2(-6, -30), Vector2(-3, -31)],
	],
	3: [
		[Vector2(-14, -39), Vector2(0, -27), Vector2(12, -26), Vector2(-13, -28)],
		[Vector2(-12, -27), Vector2(9, -37), Vector2(-16, -18), Vector2(-13, -27)],
		[Vector2(-5, -41), Vector2(-14, -39), Vector2(-29, -43), Vector2(-3, -43)],
		[Vector2(20, -36), Vector2(16, -49), Vector2(24, -57), Vector2(20, -37)],
		[Vector2(12, -46), Vector2(11, -54), Vector2(17, -53), Vector2(13, -35)],
		[Vector2(12, -46), Vector2(11, -55), Vector2(20, -54), Vector2(15, -35)],
		[Vector2(-3, -38), Vector2(17, -40), Vector2(20, -45), Vector2(-5, -33)],
		[Vector2(-12, -42), Vector2(6, -27), Vector2(13, -30), Vector2(-12, -31)],
	],
	4: [
		[Vector2(-12, -30), Vector2(-12, -56), Vector2(-2, -21), Vector2(-3, -12)],
		[Vector2(-12, -27), Vector2(-9, -64), Vector2(-9, -15), Vector2(-12, -25)],
		[Vector2(-5, -41), Vector2(3, -64), Vector2(-26, -29), Vector2(-8, -38)],
		[Vector2(20, -32), Vector2(9, -65), Vector2(-20, -43), Vector2(16, -34)],
		[Vector2(14, -26), Vector2(7, -59), Vector2(17, -27), Vector2(11, -27)],
		[Vector2(11, -30), Vector2(8, -58), Vector2(10, -36), Vector2(10, -25)],
		[Vector2(-6, -23), Vector2(-11, -61), Vector2(11, -24), Vector2(5, -11)],
		[Vector2(-12, -23), Vector2(-12, -59), Vector2(-5, -22), Vector2(-8, -10)],
	],
}

# Blade angle in screen space, keyed to the four running poses.
const RUN_ANGLES := [
	[152.0, 160.0, 132.0, 116.0],
	[159.0, 168.0, 145.0, 152.0],
	[25.0, 30.0, 12.0, 34.0],
	[35.0, 32.0, 20.0, 34.0],
	[46.0, 55.0, 66.0, 57.0],
	[136.0, 146.0, 158.0, 146.0],
	[163.0, 170.0, 180.0, 168.0],
	[231.0, 239.0, 246.0, 237.0],
]
