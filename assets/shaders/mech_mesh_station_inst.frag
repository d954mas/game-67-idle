precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.48, 0.88, 0.24));
    vec3 fill_dir = normalize(vec3(0.70, 0.44, -0.50));
    vec3 top_dir = normalize(vec3(0.04, 0.99, 0.16));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse_raw = max(dot(normal, light_dir), 0.0);
    float diffuse = mix(diffuse_raw, smoothstep(0.08, 0.94, diffuse_raw), 0.62);
    float fill = max(dot(normal, fill_dir), 0.0);
    float top = max(dot(normal, top_dir), 0.0);
    vec3 half_dir = normalize(light_dir + view_dir);
    float ndh = max(dot(normal, half_dir), 0.0);
    float specular = pow(ndh, 42.0);
    float soft_spec = pow(ndh, 8.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.25);

    float vertical = 1.0 - smoothstep(0.72, 0.98, abs(normal.y));
    float panel_x = 1.0 - smoothstep(0.025, 0.070, abs(fract(v_world_pos.x * 1.35) - 0.5));
    float panel_z = 1.0 - smoothstep(0.025, 0.070, abs(fract(v_world_pos.z * 1.35) - 0.5));
    float panel = max(panel_x, panel_z) * vertical * 0.45;
    float top_gloss = smoothstep(0.35, 0.95, normal.y);

    vec3 base = pow(clamp(v_color.rgb, 0.0, 1.0), vec3(0.62));
    vec3 accent = mix(base, vec3(0.64, 0.95, 1.0), 0.42);
    base = mix(base, accent, panel);

    vec3 lit = base * (0.48 + diffuse * 0.82 + fill * 0.28 + top * 0.18);
    lit += base * vec3(0.45, 0.90, 0.45) * clamp(1.0 - normal.y, 0.0, 1.0) * 0.18;
    lit += vec3(1.0, 0.96, 0.78) * specular * (0.42 + top_gloss * 0.30);
    lit += vec3(0.78, 0.92, 1.0) * soft_spec * 0.14;
    lit += vec3(0.70, 0.95, 1.0) * rim * 0.28;
    frag_color = vec4(clamp(pow(lit, vec3(0.86)), 0.0, 1.0), v_color.a);
}
