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
    vec3 light_dir = normalize(vec3(-0.44, 0.86, 0.26));
    vec3 fill_dir = normalize(vec3(0.72, 0.38, -0.54));
    vec3 top_dir = normalize(vec3(0.04, 0.98, 0.18));
    vec3 view_dir = normalize(camera_pos.xyz - v_world_pos);
    float diffuse_raw = max(dot(normal, light_dir), 0.0);
    float diffuse = mix(diffuse_raw, smoothstep(0.12, 0.96, diffuse_raw), 0.58);
    float fill = max(dot(normal, fill_dir), 0.0);
    float top = max(dot(normal, top_dir), 0.0);
    float ground_bounce = clamp(1.0 - normal.y, 0.0, 1.0) * 0.16;
    vec3 half_dir = normalize(light_dir + view_dir);
    float ndh = max(dot(normal, half_dir), 0.0);
    float specular = pow(ndh, 34.0);
    float broad_spec = pow(ndh, 7.0);
    float rim = pow(1.0 - max(dot(normal, view_dir), 0.0), 1.50);
    vec4 texel = texture(u_mech_texture, v_uv0);
    vec3 source_color = clamp(texel.rgb * v_color.rgb, 0.0, 1.0);
    float bright_color = max(max(source_color.r, source_color.g), source_color.b);
    float chroma = bright_color - min(min(source_color.r, source_color.g), source_color.b);
    float emissive = smoothstep(0.62, 1.0, bright_color) * smoothstep(0.22, 0.78, chroma);
    vec3 avg = vec3((source_color.r + source_color.g + source_color.b) / 3.0);
    vec3 toy_color = mix(avg, source_color, 1.18);
    vec3 base = pow(clamp(toy_color, 0.0, 1.0), vec3(0.68));
    vec3 lit = base * (0.48 + diffuse * 0.82 + fill * 0.26 + top * 0.16);
    lit += base * vec3(0.50, 0.88, 0.42) * ground_bounce;
    lit += vec3(1.0, 0.96, 0.78) * specular * 0.36;
    lit += vec3(0.70, 0.88, 1.0) * broad_spec * 0.16;
    lit += vec3(0.76, 0.94, 1.0) * rim * 0.24;
    lit += base * emissive * 0.46;
    lit = clamp(lit, 0.0, 1.0);
    lit = pow(lit, vec3(0.90));
    frag_color = vec4(lit, texel.a * v_color.a);
}
