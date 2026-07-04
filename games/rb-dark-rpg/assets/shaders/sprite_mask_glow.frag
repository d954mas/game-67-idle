precision mediump float;

uniform sampler2D u_texture;

in vec2 v_texcoord;
in vec4 v_color;

out vec4 frag_color;

void main() {
    vec4 tex = texture(u_texture, v_texcoord);
    float a = tex.a * v_color.a;
    frag_color = vec4(v_color.rgb * a, a);
}
