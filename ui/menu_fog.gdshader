shader_type canvas_item;
render_mode unshaded;

float hash(vec2 cell) {
	return fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
	vec2 cell = floor(p);
	vec2 f = fract(p);
	vec2 blend = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(hash(cell), hash(cell + vec2(1.0, 0.0)), blend.x),
		mix(hash(cell + vec2(0.0, 1.0)), hash(cell + vec2(1.0, 1.0)), blend.x),
		blend.y
	);
}

float fog_band(vec2 uv, float speed, float height, float seed) {
	vec2 flow = vec2(uv.x * 5.0 - TIME * speed, uv.y * 14.0 + seed);
	float drift = noise(vec2(flow.x * 0.68, seed));
	float distance = abs(uv.y - height - (drift - 0.5) * 0.14);
	float band = 1.0 - smoothstep(0.012, 0.12, distance);
	float density = noise(flow) * 0.72 + noise(flow * 2.1) * 0.28;
	return band * smoothstep(0.25, 0.75, density);
}

void fragment() {
	float distant = fog_band(UV, 0.21, 0.66, 3.0);
	float near = fog_band(UV, 0.34, 0.84, 11.0);
	float opacity = distant * 0.28 + near * 0.36;
	COLOR = vec4(0.71, 0.80, 0.88, opacity);
}
