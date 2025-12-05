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

struct InstanceInput {
    @location(2) model_matrix_0: vec4f,
    @location(3) model_matrix_1: vec4f,
    @location(4) model_matrix_2: vec4f,
    @location(5) model_matrix_3: vec4f,
};

struct VertexOutput{
    @builtin(position) position: vec4f,
    @location(0) normal: vec3f,
};

@vertex
fn vs_main(v: VertexInput, i: InstanceInput, @builtin(vertex_index) index: u32) -> VertexOutput {
    let model_matrix = mat4x4<f32>(
        i.model_matrix_0,
        i.model_matrix_1,
        i.model_matrix_2,
        i.model_matrix_3,
    );

    var out: VertexOutput;
    out.normal = v.normal;
    out.position = shader_data.camera_matrix * model_matrix * vec4f(v.position.xyz, 1.0f);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let color = vec3f(dot(in.normal, normalize(vec3f(1.0f, 1.0f, 1.0f))));
    return vec4f(pow(color, vec3f(2.2)), 1.0f);
}
