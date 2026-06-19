precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.44, 0.86, 0.26));
    vec3 fill_dir = normalize(vec3(0.72, 0.38, -0.54));
    vec3 top_dir = normalize(vec3(0.04, 0.98, 0.18));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse_raw = max(dot(normal, light_dir), 0.0);
    float diffuse = mix(diffuse_raw, smoothstep(0.12, 0.96, diffuse_raw), 0.50);
    float fill = max(dot(normal, fill_dir), 0.0);
    float top = max(dot(normal, top_dir), 0.0);
    float ground_bounce = clamp(1.0 - normal.y, 0.0, 1.0) * 0.14;
    vec3 half_dir = normalize(light_dir + view_dir);
    float ndh = max(dot(normal, half_dir), 0.0);
    float specular = pow(ndh, 30.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.45);
    vec3 base = pow(clamp(v_color.rgb, 0.0, 1.0), vec3(0.68));
    vec3 lit = base * (0.50 + diffuse * 0.76 + fill * 0.28 + top * 0.16);
    lit += base * vec3(0.50, 0.90, 0.42) * ground_bounce;
    lit += vec3(1.0, 0.96, 0.78) * specular * 0.32;
    lit += vec3(0.70, 0.92, 1.0) * rim * 0.22;
    frag_color = vec4(clamp(pow(lit, vec3(0.90)), 0.0, 1.0), v_color.a);
}
