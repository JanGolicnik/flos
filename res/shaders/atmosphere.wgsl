@group(0) @binding(0) var<uniform> shader_data: ShaderData;

struct Planet {
    pos: vec3f,
    radius: f32,
};

@group(1) @binding(0) var<storage, read> planets: array<Planet>;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) v_index : u32) -> VertexOutput {
    var output: VertexOutput;
    output.position = vec4(FULLSCREEN_QUAD_POSITIONS[v_index], 0.0f, 1.0f);
    output.uv = output.position.xy;
    return output;
}

// https://gist.github.com/wwwtyro/beecc31d65d1004f5a9d
fn raySphereIntersect(r0: vec3f, rd: vec3f, s0: vec3f, sr: f32) -> f32 {
    let a = dot(rd, rd);
    let s0_r0 = r0 - s0;
    let b = 2.0 * dot(rd, s0_r0);
    let c = dot(s0_r0, s0_r0) - (sr * sr);
    if (b*b - 4.0*a*c < 0.0) {
        return -1.0;
    }
    return (-b - sqrt((b*b) - 4.0*a*c))/(2.0*a);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let far = shader_data.inv_camera_matrix * vec4f(in.uv, 1.0f, 1.0f);
    let world = far.xyz / far.w;
    let r0 = shader_data.camera_position;
    let rd = normalize(world - r0);
    var color = vec3f(0.0f);
    for (var i = 0u; i < arrayLength(&planets); i++) {
        let planet = planets[i];
        if (raySphereIntersect(r0, rd, planet.pos, planet.radius + 1.5f) > 0.0f) {
            color += vec3f(0.0f, 0.4f, 1.0f);
        }
    }
    return vec4f(color, 0.2f);
}
