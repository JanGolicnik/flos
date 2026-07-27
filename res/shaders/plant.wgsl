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
};

struct VertexOutput{
    @builtin(position) position: vec4f,
    @location(0) t: f32,
};

@vertex
fn vs_main(v: VertexInput, i: InstanceInput) -> VertexOutput {
    let model = mat4x4f(i.m0, i.m1, i.m2, i.m3);
    var out: VertexOutput;
    out.position = shader_data.camera_matrix * model * vec4f(v.position.xyz, 1.0f);
    out.t = v.position.y / 2.0f;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let dark = vec3f(27, 94, 32) / 255.0;
    let light = vec3f(1.0f);

    let color = mix(dark, light, min(in.t * in.t, 1.0f));

    return vec4f(pow(color, vec3f(2.2)), 1.0f);
}
