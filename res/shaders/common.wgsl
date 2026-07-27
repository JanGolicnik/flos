struct ShaderData {
    camera_matrix: mat4x4f,
    inv_camera_matrix: mat4x4f,
    camera_position: vec3f,
    time: f32,
    res: vec2f
};

const PI = 3.14159265359;

const FULLSCREEN_QUAD_POSITIONS : array<vec2f, 6> = array<vec2f, 6>(
    vec2f(-1.0, -1.0),
    vec2f( 1.0, -1.0),
    vec2f( 1.0,  1.0),

    vec2f(-1.0, -1.0),
    vec2f( 1.0,  1.0),
    vec2f(-1.0,  1.0),
);

fn pcg_hash_u32(x: u32) -> u32 {
    let state = x * 747796405u + 2891336453u;
    let word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

fn pcg_u32_f32(v: u32) -> f32 {
    return f32(v >> 8u) * (1.0 / 16777216.0);
}

fn pcg_float(x: f32) -> f32 {
    return pcg_u32_f32(pcg_hash_u32(bitcast<u32>(x)));
}

fn pcg_vec2(p: vec2f) -> f32 {
    var h = pcg_hash_u32(bitcast<u32>(p.x));
    h = pcg_hash_u32(h ^ bitcast<u32>(p.y));
    return pcg_u32_f32(h);
}

fn pcg_vec3(p: vec3f) -> f32 {
    var h = pcg_hash_u32(bitcast<u32>(p.x));
    h = pcg_hash_u32(h ^ bitcast<u32>(p.y));
    h = pcg_hash_u32(h ^ bitcast<u32>(p.z));
    return pcg_u32_f32(h);
}

// https://www.shadertoy.com/view/ltl3D8
fn dir_to_cubemap(d: vec3f) -> vec2f {
    let n = abs(d);
    let v = select(select(d.zxy, d.yzx, n.y > n.x && n.y > n.z), d.xyz, n.x > n.y && n.x > n.z);
    let q = v.yz / v.x;
    return q * (1.25f - 0.25f * q * q);
}

fn positiveDot(a: vec3f, b: vec3f) -> f32 {
    return max(dot(a, b), 0.0f);
}

fn isnan(x: f32) -> bool {
  let highVal = 1000000.0f;
  let x2 = min(x, highVal);
  return x2 == highVal;
}

fn length2(v: vec3f) -> f32 {
    return dot(v, v);
}
