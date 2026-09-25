precision highp float;
precision highp int;

// Mode 0 is sprite.frag. Mode 1 is a rounded panel, mode 2 a disc, ring or arc;
// both are signed distances in layout units, antialiased over one screen pixel
// through fwidth, so they stay crisp at any UI scale and under rotation. Every
// branch below tests a flat input, so derivatives stay defined.
// Packing and field meaning: features/ui-kit/include/features/ui_kit/ui_shape_pack.h.

uniform sampler2D u_texture;

in vec2 v_texcoord;
in vec4 v_color;
in vec2 v_uv;
flat in vec4 v_layout;
flat in vec4 v_shape;
flat in vec4 v_paint;
flat in vec4 v_fx;

out vec4 frag_color;

const float TAU = 6.28318530717958647692;
const float QUARTER = 0.25;

// Every packed field is an integer below 2^24, exact in float32: split it with
// floor, never with bit casts (some ANGLE paths canonicalise NaN patterns).
vec2 split12(float v) {
    float hi = floor(v / 4096.0);
    return vec2(v - hi * 4096.0, hi);
}

vec3 split8(float v) {
    float hi = floor(v / 65536.0);
    float rest = v - hi * 65536.0;
    float mid = floor(rest / 256.0);
    return vec3(rest - mid * 256.0, mid, hi);
}

// r = tl, tr, br, bl; p is centred, y down.
float sd_rbox(vec2 p, vec2 half_sz, vec4 r) {
    float rr = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);
    rr = min(rr, max(min(half_sz.x, half_sz.y), 0.0));
    vec2 q = abs(p) - half_sz + rr;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rr;
}

float cov(float d) {
    return clamp(0.5 - d / max(fwidth(d), 1e-4), 0.0, 1.0);
}

// Coverage of the unit disc for a normalised radius, as radial.frag computes it.
float disc_cov(float r) {
    return clamp((1.0 - r) / max(fwidth(r), 1e-6) + 0.5, 0.0, 1.0);
}

vec4 over(vec4 below, vec3 rgb, float a) {
    return vec4(rgb * a, a) + below * (1.0 - a);
}

vec4 panel(vec2 size, vec2 px) {
    vec2 mode_outline = split12(v_shape.x);
    float outline = mode_outline.x * QUARTER;
    vec2 r01 = split12(v_shape.y) * QUARTER;
    vec2 r23 = split12(v_shape.z) * QUARTER;
    vec4 radius = vec4(r01.x, r01.y, r23.x, r23.y);
    vec2 face_lip = split12(v_shape.w) * QUARTER;
    vec3 bottom = split8(v_paint.x) / 255.0;
    vec3 outline_rgb = split8(v_paint.y) / 255.0;
    vec3 lip_rgb = split8(v_paint.z) / 255.0;
    vec3 alphas = split8(v_paint.w) / 255.0;
    vec2 gloss_at = split12(v_fx.x) * QUARTER;
    vec2 gloss_size = split12(v_fx.y) * QUARTER;
    vec2 checker_hl = split12(v_fx.z) * QUARTER;
    vec2 press_flags = split12(v_fx.w);
    float lip = face_lip.y;
    float press = press_flags.x / 255.0 * lip;
    bool overlay = mod(press_flags.y, 2.0) >= 1.0;

    // The face stands on the lip: the element's box is the face plus the lip
    // under it, and a press sinks the face onto the lip.
    float face_h = size.y - lip;
    vec2 face_c = vec2(size.x * 0.5, press + face_h * 0.5);
    vec2 face_half = vec2(size.x, face_h) * 0.5;
    vec4 inner_r = face_lip.x > 0.0 ? vec4(face_lip.x) : max(radius - outline, 0.0);
    float d_face = sd_rbox(px - face_c, face_half, radius);
    float d_in = sd_rbox(px - face_c, face_half - outline, inner_r);

    vec4 acc = vec4(0.0);
    if (!overlay) {
        if (lip > 0.0) {
            acc = over(acc, lip_rgb, cov(sd_rbox(px - size * 0.5, size * 0.5, radius)));
        }
        if (outline > 0.0) {
            acc = over(acc, outline_rgb, cov(d_face));
        }
        float t = clamp((px.y - press - outline) / max(face_h - 2.0 * outline, 1e-3), 0.0, 1.0);
        acc = over(acc, mix(v_color.rgb, bottom, t), cov(d_in));
    }

    // White cells over the inside, kept out of its rounded corners and cut at
    // its edges; hard-edged, as rectangles on the pixel grid would be.
    float cell = checker_hl.x;
    if (alphas.y > 0.0 && cell > 0.0) {
        vec2 inner = vec2(size.x, face_h) - 2.0 * outline;
        vec2 q = px - vec2(outline, press + outline);
        vec2 c = floor(q / cell);
        vec2 lo = c * cell;
        vec2 hi = min(lo + cell, inner);
        float keep_out = max(max(inner_r.x, inner_r.y), max(inner_r.z, inner_r.w));
        bool side = lo.x < keep_out || hi.x > inner.x - keep_out;
        bool end = lo.y < keep_out || hi.y > inner.y - keep_out;
        bool inside = all(greaterThanEqual(q, vec2(0.0))) && all(lessThan(q, inner));
        bool on = mod(c.x + c.y, 2.0) < 0.5;
        if (inside && on && !(side && end)) {
            acc = over(acc, vec3(1.0), alphas.y);
        }
    }

    // The gloss: a white band whose ends are half-ellipses gloss_size.y wide.
    if (alphas.x > 0.0 && gloss_size.x > 0.0) {
        float left = outline + gloss_at.x;
        float half_w = max((size.x - 2.0 * left) * 0.5, 0.0);
        float half_h = gloss_size.x * 0.5;
        float cap = gloss_size.y > 0.0 ? min(gloss_size.y, half_w) : half_h;
        float squash = half_h / max(cap, 1e-3);
        vec2 g = px - vec2(size.x * 0.5, press + outline + gloss_at.y + half_h);
        g.x *= squash;
        acc = over(acc, vec3(1.0), alphas.x * cov(sd_rbox(g, vec2(half_w * squash, half_h), vec4(half_h))));
    }

    // Inner highlight: a band just inside the outline.
    if (alphas.z > 0.0 && checker_hl.y > 0.0) {
        float band = cov(d_face + outline) * (1.0 - cov(d_face + outline + checker_hl.y));
        acc = over(acc, vec3(1.0), alphas.z * band);
    }
    return acc;
}

vec4 radial(vec2 size, vec2 px) {
    float outline = split12(v_shape.x).x * QUARTER;
    float angle_start = v_shape.y;
    float angle_end = v_shape.z;
    float inner_norm = v_shape.w;
    vec3 bottom = split8(v_paint.x) / 255.0;
    vec3 outline_rgb = split8(v_paint.y) / 255.0;
    vec3 lip_rgb = split8(v_paint.z) / 255.0;
    float gloss_a = split8(v_paint.w).x / 255.0;
    float lip = split12(v_fx.x).x * QUARTER;
    bool overlay = mod(split12(v_fx.w).y, 2.0) >= 1.0;

    // The face ellipse fills the box above the lip; the lip is the ellipse of
    // the whole box, so under the face it shows as a crescent.
    float face_h = size.y - lip;
    vec2 c = vec2(size.x, face_h) * 0.5;
    vec2 rad = vec2(size.x, face_h) * 0.5;
    vec2 p = (px - c) / rad;
    float r = length(p);
    float ppu = 1.0 / max(fwidth(r), 1e-6);
    float outer = clamp((1.0 - r) * ppu + 0.5, 0.0, 1.0);
    float ring = inner_norm > 0.0 ? clamp((r - inner_norm) * ppu + 0.5, 0.0, 1.0) : 1.0;

    // y-up angles, 0 = +X, counter-clockwise; the wrap-aware wedge of radial.frag.
    float ang = atan(-p.y, p.x);
    float sweep = mod(ang - angle_start, TAU);
    float total = mod(angle_end - angle_start, TAU);
    bool full_turn = abs(angle_end - angle_start) >= TAU - 1e-4;
    float lead = clamp(r * sweep * ppu + 0.5, 0.0, 1.0);
    float trail = clamp(r * (total - sweep) * ppu + 0.5, 0.0, 1.0);
    float wedge = full_turn ? 1.0 : lead * trail;

    vec2 rad_in = max(rad - outline, vec2(1e-3));
    vec4 acc = vec4(0.0);
    if (!overlay) {
        if (lip > 0.0) {
            acc = over(acc, lip_rgb, disc_cov(length((px - size * 0.5) / (size * 0.5))));
        }
        float fill_cov = outer;
        if (outline > 0.0) {
            acc = over(acc, outline_rgb, outer);
            fill_cov = disc_cov(length((px - c) / rad_in));
        }
        float t = clamp((px.y - (c.y - rad_in.y)) / (2.0 * rad_in.y), 0.0, 1.0);
        acc = over(acc, mix(v_color.rgb, bottom, t), fill_cov);
    }
    if (gloss_a > 0.0) {
        // 72% across, 30% tall, 6% below the top of the inside.
        float d = 2.0 * rad_in.y;
        vec2 gc = vec2(c.x, c.y - rad_in.y + d * 0.21);
        acc = over(acc, vec3(1.0), gloss_a * disc_cov(length((px - gc) / (vec2(0.36, 0.15) * d))));
    }
    return acc * ring * wedge;
}

void main() {
    float mode = split12(v_shape.x).y;
    if (mode < 0.5) {
        vec4 tex = texture(u_texture, v_texcoord);
        frag_color = tex * vec4(v_color.rgb * v_color.a, v_color.a);
        return;
    }
    vec2 size = v_layout.yz;
    vec2 px = v_uv * size;
    vec4 acc = mode < 1.5 ? panel(size, px) : radial(size, px);
    float a = acc.a * v_color.a;
    if (a <= 0.0) {
        discard;
    }
    frag_color = acc * v_color.a;
}
