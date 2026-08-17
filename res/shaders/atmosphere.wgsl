@group(0) @binding(0) var<uniform> shader_data: ShaderData;

struct Planet {
    pos: vec3f,
    radius: f32,
};

@group(1) @binding(0) var<storage, read> planets: array<Planet>;
@group(1) @binding(1) var depthTexture: texture_depth_2d;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) ndc: vec2f,
}

@vertex
fn vs_main(@builtin(vertex_index) v_index : u32) -> VertexOutput {
    let v = FULLSCREEN_QUAD_POSITIONS[v_index];
    var output: VertexOutput;
    output.position = vec4(v, 0.0f, 1.0f);
    output.uv = v * 0.5f + 0.5f;
    output.uv.y = 1.0f - output.uv.y;
    output.ndc = output.position.xy;
    return output;
}

// https://gist.github.com/wwwtyro/beecc31d65d1004f5a9d
// fn raySphereIntersect(r0: vec3f, rd: vec3f, s0: vec3f, sr: f32) -> f32 {
//     let a = dot(rd, rd);
//     let s0_r0 = r0 - s0;
//     let b = 2.0 * dot(rd, s0_r0);
//     let c = dot(s0_r0, s0_r0) - (sr * sr);
//     if (b*b - 4.0*a*c < 0.0) {
//         return -1.0;
//     }
//     return (-b - sqrt((b*b) - 4.0*a*c))/(2.0*a);
// }

// https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes//ray-sphere-intersection.html
fn raySphereIntersect(r0: vec3f, rd: vec3f, s0: vec3f, sr: f32, _t0: ptr<function, f32>, _t1: ptr<function, f32>) -> bool
{
    let l = s0 - r0;
    let tca = dot(l, rd);
    // if (tca < 0) return false;
    let d2 = dot(l, l) - tca * tca;
    if (d2 > sr * sr) {
        return false;
    }
    let thc = sqrt(sr * sr - d2);
    *_t0 = tca - thc;
    *_t1 = tca + thc;
    return true;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let far = shader_data.inv_camera_matrix * vec4f(in.ndc, 1.0f, 1.0f);
    let world = far.xyz / far.w;
    let r0 = shader_data.camera_position;
    let rd = normalize(world - r0);

    let loc = vec2i(in.uv * vec2f(textureDimensions(depthTexture)));
    let depth = textureLoad(depthTexture, loc, 0);
    var actual_world = shader_data.inv_camera_matrix * vec4f(in.uv, depth, 0.0f);
    actual_world = actual_world / actual_world.w;
    let scene_dist = length(actual_world.xyz - r0);

    let color1 = vec3f(0.0f, 0.0f, 1.0f);
    let color2 = vec3f(1.0f);
    // let color2 = vec3f(1.0f, 0.0f, 0.0f);
    let height = shader_data.atmosphere_height;

    var color = vec3f(0.0f);
    var scatter = 0.0f;

    for (var i = 0u; i < arrayLength(&planets); i++) {
        let planet = planets[i];
        let r = planet.radius;
        let atmo_r = r + height;

        var t0 = 0.0f; var t1 = 0.0f;
        if (!raySphereIntersect(r0, rd, planet.pos, atmo_r, &t0, &t1) || t1 < 0.0f) {
            continue;
        }

        t0 = max(t0, 0.0f);
        if (depth < 1.0f) {
            t1 = min(t1, scene_dist);
        }
        if (t1 <= t0) {
            continue;
        }

        let dist = max(t1 - t0, 0.0f);

        let limb = 2.0f * sqrt(atmo_r * atmo_r - r * r);
        let density = shader_data.atmosphere_density / limb;
        let t = 1.0f - exp(-dist * density);

        let alpha = pow(t, shader_data.atmosphere_falloff);

        color += mix(color1, color2, t) * alpha;
        scatter = scatter + alpha - scatter * alpha;
    }

    return vec4f(color * scatter, 0.1f);
}
