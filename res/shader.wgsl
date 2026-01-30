struct ShaderData {
    camera_matrix: mat4x4<f32>,
    camera_position: vec3f,
    time: f32,
};
@group(0) @binding(0) var<uniform> shader_data: ShaderData;

struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
};

struct VertexOutput{
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@vertex
fn vs_main(v: VertexInput, @builtin(vertex_index) index: u32) -> VertexOutput {
    var out: VertexOutput;
    out.normal = v.normal;
    out.position = shader_data.camera_matrix * vec4f(v.position.xyz, 1.0f);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let dark = vec3f(84, 119, 146) / 255.0;
    let light = vec3f(250, 185, 91) / 255.0f;

    let d = dot(in.normal, normalize(vec3f(1.0f, 1.0f, 1.0f)));
    let color = mix(dark, light, d * 0.5 + 0.5);

    return vec4f(pow(color, vec3f(2.2)), 1.0f);
}
