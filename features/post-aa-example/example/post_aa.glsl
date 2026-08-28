uniform sampler2D u_scene;

float post_aa_luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 post_aa_fxaa(vec2 uv) {
    vec2 px = 1.0 / vec2(textureSize(u_scene, 0));
    vec3 m = texture(u_scene, uv).rgb;
    vec3 nw = texture(u_scene, uv + px * vec2(-1.0, -1.0)).rgb;
    vec3 ne = texture(u_scene, uv + px * vec2( 1.0, -1.0)).rgb;
    vec3 sw = texture(u_scene, uv + px * vec2(-1.0,  1.0)).rgb;
    vec3 se = texture(u_scene, uv + px * vec2( 1.0,  1.0)).rgb;
    float lm = post_aa_luma(m);
    float lnw = post_aa_luma(nw);
    float lne = post_aa_luma(ne);
    float lsw = post_aa_luma(sw);
    float lse = post_aa_luma(se);
    float lo = min(lm, min(min(lnw, lne), min(lsw, lse)));
    float hi = max(lm, max(max(lnw, lne), max(lsw, lse)));
    if (hi - lo < max(0.0312, hi * 0.125)) {
        return m;
    }
    vec2 dir = vec2(-((lnw + lne) - (lsw + lse)),
                     ((lnw + lsw) - (lne + lse)));
    float reduce = max((lnw + lne + lsw + lse) * 0.0078125, 0.0009765625);
    float scale = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * scale, vec2(-8.0), vec2(8.0)) * px;
    vec3 a = 0.5 * (texture(u_scene, uv + dir * -0.166667).rgb
                  + texture(u_scene, uv + dir *  0.166667).rgb);
    vec3 b = a * 0.5 + 0.25 * (texture(u_scene, uv + dir * -0.5).rgb
                              + texture(u_scene, uv + dir *  0.5).rgb);
    float lb = post_aa_luma(b);
    return (lb < lo || lb > hi) ? a : b;
}

vec3 post_aa_box_downsample(vec2 uv, vec2 present_size) {
    vec2 offset = 0.25 / present_size;
    return 0.25 * (texture(u_scene, uv + vec2(-offset.x, -offset.y)).rgb
                 + texture(u_scene, uv + vec2( offset.x, -offset.y)).rgb
                 + texture(u_scene, uv + vec2(-offset.x,  offset.y)).rgb
                 + texture(u_scene, uv + vec2( offset.x,  offset.y)).rgb);
}
