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
    var t0 = tca - thc;
    var t1 = tca + thc;

    if (t0 < 0 && t1 < 0) {
        return false;
    }

    *_t0 = t0;
    *_t1 = t1;

    return true;
}


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let far = shader_data.inv_camera_matrix * vec4f(in.uv, 1.0f, 1.0f);
    let world = far.xyz / far.w;
    let r0 = shader_data.camera_position;
    let rd = normalize(world - r0);

    let color1 = vec3f(1.0f);
    let color2 = vec3f(1.0f, 0.0f, 0.0f);

    let height = shader_data.atmosphere_height;
    var color = vec3f(0.0f);
    var scatter = 0.0f;
    for (var i = 0u; i < arrayLength(&planets); i++) {
        let planet = planets[i];
        let r = planet.radius;
        let atmo_r = r + height;
        var t0 = 0.0f;
        var t1 = 0.0f;
        if (raySphereIntersect(r0, rd, planet.pos, atmo_r, &t0, &t1)) {
            var t2 = 0.0f;
            raySphereIntersect(r0, rd, planet.pos, r, &t1, &t2);

            let limb = 2.0f * sqrt(atmo_r*atmo_r - r*r);
            let p0 = select(r0, r0 + rd * t0, t0 > 0.0f);
            let p1 = r0 + rd * t1;
            var t = distance(p0, p1) / limb;
            t = pow(t, shader_data.atmosphere_falloff);
            color += mix(color1, color2, t) * t;
            scatter = scatter + t - scatter * t;
        }
    }
    return vec4f(color, scatter * 0.5f);
}
