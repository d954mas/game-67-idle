precision highp float;

uniform sampler2D u_texture;

in vec2 v_texcoord;
in vec4 v_color;
in vec4 v_fade;
in float v_local_y;

out vec4 frag_color;

void main() {
    vec4 tex = texture(u_texture, v_texcoord);
    float range_y = v_fade.y - v_fade.x;
    float t = clamp((v_local_y - v_fade.x) / range_y, 0.0, 1.0);
    t = t * t * (3.0 - 2.0 * t);
    float alpha = v_color.a * v_fade.z * t;
    vec4 c = vec4(v_color.rgb * alpha, alpha);
    frag_color = tex * c;
}
