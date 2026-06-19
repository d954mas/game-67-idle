precision mediump float;

#include "common/globals.glsl"

in vec3 v_normal;
in vec3 v_world_pos;
in vec2 v_uv0;
in vec4 v_color;

out vec4 frag_color;

uniform sampler2D u_mech_texture;

void main() {
    vec3 normal = normalize(v_normal);
    vec3 light_dir = normalize(vec3(-0.38, 0.88, 0.30));
    vec3 fill_dir = normalize(vec3(0.70, 0.32, -0.55));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse_raw = max(dot(normal, light_dir), 0.0);
    float diffuse = mix(diffuse_raw, smoothstep(0.18, 0.92, diffuse_raw), 0.45);
    float fill = max(dot(normal, fill_dir), 0.0);
    vec3 half_dir = normalize(light_dir + view_dir);
    float specular = pow(max(dot(normal, half_dir), 0.0), 24.0);
    float broad_spec = pow(max(dot(normal, half_dir), 0.0), 6.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.75);
    vec4 texel = texture(u_mech_texture, v_uv0);
    vec3 source_color = clamp(texel.rgb * v_color.rgb, 0.0, 1.0);
    float bright_color = max(max(source_color.r, source_color.g), source_color.b);
    float chroma = bright_color - min(min(source_color.r, source_color.g), source_color.b);
    float emissive = smoothstep(0.62, 1.0, bright_color) * smoothstep(0.22, 0.78, chroma);
    vec3 avg = vec3((source_color.r + source_color.g + source_color.b) / 3.0);
    vec3 toy_color = mix(avg, source_color, 1.18);
    vec3 base = pow(clamp(toy_color, 0.0, 1.0), vec3(0.72));
    vec3 lit = base * (0.54 + diffuse * 0.72 + fill * 0.24);
    lit += vec3(1.0, 0.96, 0.82) * specular * 0.28;
    lit += vec3(0.72, 0.88, 1.0) * broad_spec * 0.13;
    lit += vec3(0.82, 0.95, 1.0) * rim * 0.16;
    lit += base * emissive * 0.38;
    lit = clamp(lit, 0.0, 1.0);
    lit = pow(lit, vec3(0.92));
    frag_color = vec4(lit, texel.a * v_color.a);
}
