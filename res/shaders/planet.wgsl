@group(0) @binding(0) var<uniform> shader_data: ShaderData;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct InstanceInput {
    @location(3) m0: vec4f,
    @location(4) m1: vec4f,
    @location(5) m2: vec4f,
    @location(6) m3: vec4f,
    @location(7) shell_t: f32,
    @location(8) scale: f32,
};

struct VertexOutput{
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
    @location(1) shell_t: f32,
    @location(2) world: vec3f,
    @location(3) scale: f32,
};

@vertex
fn vs_main(v: VertexInput, i: InstanceInput) -> VertexOutput {
    var model = mat4x4f(i.m0, i.m1, i.m2, i.m3);
    model[0][0] += i.shell_t * 0.05f;
    model[1][1] += i.shell_t * 0.05f;
    model[2][2] += i.shell_t * 0.05f;
    let world = model * vec4f(v.position.xyz, 1.0f);
    var out: VertexOutput;
    out.position = shader_data.camera_matrix * world;
    out.normal = v.normal;
    out.shell_t = f32(i.shell_t);
    out.world = world.xyz;
    out.scale = i.scale;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let c = dir_to_cubemap(in.normal);
    let hash = pcg_vec2(floor(c * 75.0f * in.scale));
    if (hash < in.shell_t){
        discard;
    }

    let dark = vec3f(27, 94, 32) / 255.0;
    let light = vec3f(165, 214, 167) / 255.0f;

    let d = dot(in.normal, normalize(vec3f(1.0f, 1.0f, 1.0f))) * 0.5f + 0.5f;
    let color = mix(dark, light, d * in.shell_t);

    return vec4f(pow(color, vec3f(2.2)), 1.0f);
}
