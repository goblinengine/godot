/* clang-format off */
#[modes]

mode_final = #define MODE_FINAL
mode_pass1 = #define MODE_PASS1
mode_final_soft = #define MODE_FINAL_SOFT

#[specializations]

USE_GLOW = false
USE_LUMINANCE_MULTIPLIER = false
USE_BCS = false
USE_COLOR_CORRECTION = false
USE_1D_LUT = false
USE_SSAO_ABYSS = false
USE_SSAO_LOW = false
USE_SSAO_MED = false
USE_SSAO_HIGH = false
USE_SSAO_MEGA = false

#[vertex]
layout(location = 0) in vec2 vertex_attrib;

/* clang-format on */

out vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib * 0.5 + 0.5;
	gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

/* clang-format off */
#[fragment]
/* clang-format on */

// CUT (Cheap Upscaling Triangulation) upscalers - clean-room implementation
// (ADR 0009). Data-dependent triangulation on the luma plane per Su & Willis
// 2004 with Reshetov 2009-style neighbor pattern recognition. All CUT taps use
// point (nearest) sampling - the algorithm computes its own blend weights.
//
// Modes:
//   MODE_FINAL       CUT1: 1 pass, 4 samples, 45 degree resolution.
//   MODE_PASS1       CUT2 pass 1: 12 samples at input resolution. Soft-edge
//                    sharpening + per-square edge descriptor (alpha channel).
//   MODE_FINAL_SOFT  CUT2 final: 5 samples + state-driven reconstruction.
//
// The final modes carry the full post composition (tonemap + glow + SSAO +
// luminance multiplier + BCS + color correction), equivalent to post.glsl
// MODE_DEFAULT with CUT interpolation replacing the bilinear fetch.

// If we reach this code, we always tonemap.
#define APPLY_TONEMAPPING

#include "../tonemap_inc.glsl"

uniform sampler2D source_color; // texunit:0

uniform float view;
uniform float luminance_multiplier;

#ifdef USE_GLOW
uniform sampler2D glow_color; // texunit:1
uniform vec2 pixel_size;
uniform float glow_intensity;
uniform float srgb_white;

vec4 get_glow_color(vec2 uv) {
	vec2 half_pixel = pixel_size * 0.5;

	vec4 color = textureLod(glow_color, uv + vec2(-half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(glow_color, uv + vec2(-half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(glow_color, uv + vec2(0.0, half_pixel.y * 2.0), 0.0);
	color += textureLod(glow_color, uv + vec2(half_pixel.x, half_pixel.y), 0.0) * 2.0;
	color += textureLod(glow_color, uv + vec2(half_pixel.x * 2.0, 0.0), 0.0);
	color += textureLod(glow_color, uv + vec2(half_pixel.x, -half_pixel.y), 0.0) * 2.0;
	color += textureLod(glow_color, uv + vec2(0.0, -half_pixel.y * 2.0), 0.0);
	color += textureLod(glow_color, uv + vec2(-half_pixel.x, -half_pixel.y), 0.0) * 2.0;

#ifdef USE_LUMINANCE_MULTIPLIER
	color = color / luminance_multiplier;
#endif

	return color / 12.0;
}
#endif // USE_GLOW

#ifdef USE_COLOR_CORRECTION
#ifdef USE_1D_LUT
uniform sampler2D source_color_correction; //texunit:2

vec3 apply_color_correction(vec3 color) {
	color.r = texture(source_color_correction, vec2(color.r, 0.0f)).r;
	color.g = texture(source_color_correction, vec2(color.g, 0.0f)).g;
	color.b = texture(source_color_correction, vec2(color.b, 0.0f)).b;
	return color;
}
#else
uniform sampler3D source_color_correction; //texunit:2

vec3 apply_color_correction(vec3 color) {
	return textureLod(source_color_correction, color, 0.0).rgb;
}
#endif // USE_1D_LUT
#endif // USE_COLOR_CORRECTION

#if defined(USE_SSAO_ABYSS) || defined(USE_SSAO_LOW) || defined(USE_SSAO_MED) || defined(USE_SSAO_HIGH) || defined(USE_SSAO_MEGA)
#define USE_SOME_SSAO
uniform float ssao_intensity;
uniform float ssao_radius_frac;
uniform vec2 ssao_prn_UV;
uniform sampler2D depth_buffer; // texunit:3
#if defined(USE_SSAO_ABYSS)
// Use the tiny 2-sample version.
#include "../s4ao_micro_inc.glsl"
#elif defined(USE_SSAO_HIGH) || defined(USE_SSAO_MEGA)
// Use the rings version for the higher qualities.
#include "../s4ao_mega_inc.glsl"
#else
// Use the more generic NxN grid version.
#include "../s4ao_inc.glsl"
#endif
#endif

// --- CUT parameters (registered under rendering/scaling_3d/cut_*) ---

uniform vec2 source_size;
uniform float cut_blend_sharpness;
uniform float cut_edge_min_value;
uniform float cut_fast_luma;
uniform float cut_soft_threshold;
uniform float cut_sharpening_amount;

in vec2 uv_interp;

layout(location = 0) out vec4 frag_color;

float cut_luma(vec3 color) {
	float fast_luma = (color.r + color.g + color.b) * 0.3333333333;
	float rec601_luma = dot(color, vec3(0.299, 0.587, 0.114));
	return mix(rec601_luma, fast_luma, cut_fast_luma);
}

// Edge descriptor packing (alpha channel of the intermediate buffers):
//   bits 5-7: orientation class (0 = flat, 1 = diagonal a-d, 2 = diagonal b-c,
//             3 = horizontal, 4 = vertical)
//   bits 0-4: edge strength [0,1]
float cut_pack_descriptor(float p_class, float p_strength) {
	return (p_class * 32.0 + p_strength * 31.0) / 255.0;
}

float cut_unpack_class(float p_descriptor) {
	return floor(p_descriptor * 255.0 / 32.0);
}

float cut_unpack_strength(float p_descriptor) {
	float a8 = p_descriptor * 255.0;
	return (a8 - floor(a8 / 32.0) * 32.0) / 31.0;
}

// CUT interpolation inside one 2x2 source square. p_uv is the output position
// in [0,1]; the final passes replace the bilinear fetch with this.
vec4 cut_interpolate(vec2 p_uv) {
	vec2 fpos = p_uv * source_size - 0.5; // Position in texel-center space.
	vec2 base = floor(fpos);
	vec2 frac = fpos - base; // Position inside the 2x2 square [0,1).

	vec2 uvb = (base + 0.5) / source_size;
	float inv_w = 1.0 / source_size.x;
	float inv_h = 1.0 / source_size.y;

	vec4 a = texture(source_color, uvb); // Top-left.
	vec4 b = texture(source_color, uvb + vec2(inv_w, 0.0)); // Top-right.
	vec4 c = texture(source_color, uvb + vec2(0.0, inv_h)); // Bottom-left.
	vec4 d = texture(source_color, uvb + vec2(inv_w, inv_h)); // Bottom-right.

	float la = cut_luma(a.rgb);
	float lb = cut_luma(b.rgb);
	float lc = cut_luma(c.rgb);
	float ld = cut_luma(d.rgb);

	vec4 bilinear_color = mix(mix(a, b, frac.x), mix(c, d, frac.x), frac.y);

	float d1 = abs(la - ld); // Diagonal a-d contrast.
	float d2 = abs(lb - lc); // Diagonal b-c contrast.
	float edge = max(d1, d2);
	if (edge < cut_edge_min_value) {
		return bilinear_color; // Flat square: no edge to follow.
	}

	vec4 cut_color = bilinear_color;
	float sharpness = cut_blend_sharpness;

#ifdef MODE_FINAL_SOFT
	// Fifth sample: the square's edge descriptor, stored in the alpha channel
	// of the top-left texel by MODE_PASS1.
	vec4 state = texture(source_color, uvb);
	float s_cls = cut_unpack_class(state.a);
	float strength = cut_unpack_strength(state.a);
	int cls = 1;
	if (s_cls >= 1.0 && s_cls <= 4.0) {
		cls = int(s_cls);
	} else {
		cls = (d1 <= d2) ? 1 : 2;
	}
	// Soft (antialiased) edges keep a softer reconstruction.
	sharpness = mix(cut_blend_sharpness, 1.0, strength);
#else
	// CUT1: orientation handling on the luma plane - horizontal and vertical
	// edges get a sharp step, diagonals use the data-dependent triangulation.
	float eh = abs((la + lb) - (lc + ld)); // Horizontal edge energy.
	float ev = abs((la + lc) - (lb + ld)); // Vertical edge energy.
	int cls;
	if (eh >= ev && eh >= d1 && eh >= d2) {
		cls = 3; // Horizontal.
	} else if (ev >= eh && ev >= d1 && ev >= d2) {
		cls = 4; // Vertical.
	} else {
		cls = (d1 <= d2) ? 1 : 2; // Diagonal.
	}
#endif

	if (cls == 3) {
		// Horizontal edge: step across the middle row boundary.
		cut_color = (frac.y < 0.5) ? mix(a, b, 0.5) : mix(c, d, 0.5);
	} else if (cls == 4) {
		// Vertical edge: step across the middle column boundary.
		cut_color = (frac.x < 0.5) ? mix(a, c, 0.5) : mix(b, d, 0.5);
	} else if (cls == 2) {
		// Diagonal b-c: triangles (a,b,c) and (b,c,d).
		if (frac.x + frac.y <= 1.0) {
			cut_color = a * (1.0 - frac.x - frac.y) + b * frac.x + c * frac.y;
		} else {
			cut_color = d * (frac.x + frac.y - 1.0) + c * (1.0 - frac.x) + b * (1.0 - frac.y);
		}
	} else {
		// Diagonal a-d: triangles (a,b,d) and (a,c,d).
		if (frac.x >= frac.y) {
			cut_color = a * (1.0 - frac.x) + b * (frac.x - frac.y) + d * frac.y;
		} else {
			cut_color = a * (1.0 - frac.y) + d * frac.x + c * (frac.y - frac.x);
		}
	}

	return mix(bilinear_color, cut_color, sharpness);
}

void main() {
#ifdef MODE_PASS1
	// Intermediate pass: one output texel per input texel.
	vec2 fpos = uv_interp * source_size - 0.5;
	vec2 base = floor(fpos);
	vec2 uvb = (base + 0.5) / source_size;
	float inv_w = 1.0 / source_size.x;
	float inv_h = 1.0 / source_size.y;
#endif

#ifdef MODE_PASS1
	// 2x2 square plus its 8 side-adjacent neighbors (12 samples). Computes the
	// square's edge orientation and soft-edge sharpening, then packs both into
	// the output alpha channel.
	{
		vec4 a = texture(source_color, uvb);
		vec4 b = texture(source_color, uvb + vec2(inv_w, 0.0));
		vec4 c = texture(source_color, uvb + vec2(0.0, inv_h));
		vec4 d = texture(source_color, uvb + vec2(inv_w, inv_h));
		vec4 tN = texture(source_color, uvb + vec2(0.0, -inv_h));
		vec4 tNE = texture(source_color, uvb + vec2(inv_w, -inv_h));
		vec4 tE = texture(source_color, uvb + vec2(2.0 * inv_w, 0.0));
		vec4 tES = texture(source_color, uvb + vec2(2.0 * inv_w, inv_h));
		vec4 tS = texture(source_color, uvb + vec2(inv_w, 2.0 * inv_h));
		vec4 tSW = texture(source_color, uvb + vec2(0.0, 2.0 * inv_h));
		vec4 tW = texture(source_color, uvb + vec2(-inv_w, inv_h));
		vec4 tWN = texture(source_color, uvb + vec2(-inv_w, 0.0));

		float la = cut_luma(a.rgb);
		float lb = cut_luma(b.rgb);
		float lc = cut_luma(c.rgb);
		float ld = cut_luma(d.rgb);
		float ln = cut_luma(tN.rgb);
		float lne = cut_luma(tNE.rgb);
		float le = cut_luma(tE.rgb);
		float les = cut_luma(tES.rgb);
		float ls = cut_luma(tS.rgb);
		float lsw = cut_luma(tSW.rgb);
		float lw = cut_luma(tW.rgb);
		float lwn = cut_luma(tWN.rgb);

		float d1 = abs(la - ld);
		float d2 = abs(lb - lc);
		float eh = abs((la + lb) - (lc + ld)); // Horizontal edge energy.
		float ev = abs((la + lc) - (lb + ld)); // Vertical edge energy.
		float sq_contrast = max(max(d1, d2), max(eh, ev));

		// Orientation classification.
		float cls = 0.0; // Flat.
		if (sq_contrast >= cut_edge_min_value) {
			if (eh >= ev && eh >= d1 && eh >= d2) {
				cls = 3.0; // Horizontal.
			} else if (ev >= eh && ev >= d1 && ev >= d2) {
				cls = 4.0; // Vertical.
			} else if (d1 <= d2) {
				cls = 1.0; // Diagonal a-d.
			} else {
				cls = 2.0; // Diagonal b-c.
			}
		}

		// Soft-edge handling: an antialiased pixel sits between the two edge
		// colors; a strong (hard) pixel sits at one of them. Antialiased pixels
		// are pulled toward the dominant cluster to enhance edge definition.
		vec3 sharpened = a.rgb;
		float strength = clamp(sq_contrast, 0.0, 1.0);
		if (sq_contrast >= cut_edge_min_value) {
			float lo = min(min(min(ln, lne), min(le, les)), min(ls, min(lsw, min(lw, lwn))));
			float hi = max(max(max(ln, lne), max(le, les)), max(ls, max(lsw, max(lw, lwn))));
			float ring_contrast = hi - lo;
			if (ring_contrast >= cut_edge_min_value) {
				float mid = (hi + lo) * 0.5;
				float dev = abs(la - mid);
				// Relative score: 1 = strong (hard) edge pixel, 0 = perfectly antialiased.
				float score = clamp(dev / (ring_contrast * 0.5), 0.0, 1.0);
				// Smooth softness over the whole antialiased band (not just the
				// exact midpoint pixels): the band's outer pixels score ~4x the
				// HARD_EDGES_THRESHOLD, so normalize against that.
				float softness = 1.0 - clamp(score / max(cut_soft_threshold * 4.0, 0.001), 0.0, 1.0);
				if (softness > 0.001) {
					vec3 light_cluster = vec3(0.0);
					vec3 dark_cluster = vec3(0.0);
					float light_n = 0.0;
					float dark_n = 0.0;
					if (ln >= mid) { light_cluster += tN.rgb; light_n += 1.0; } else { dark_cluster += tN.rgb; dark_n += 1.0; }
					if (lne >= mid) { light_cluster += tNE.rgb; light_n += 1.0; } else { dark_cluster += tNE.rgb; dark_n += 1.0; }
					if (le >= mid) { light_cluster += tE.rgb; light_n += 1.0; } else { dark_cluster += tE.rgb; dark_n += 1.0; }
					if (les >= mid) { light_cluster += tES.rgb; light_n += 1.0; } else { dark_cluster += tES.rgb; dark_n += 1.0; }
					if (ls >= mid) { light_cluster += tS.rgb; light_n += 1.0; } else { dark_cluster += tS.rgb; dark_n += 1.0; }
					if (lsw >= mid) { light_cluster += tSW.rgb; light_n += 1.0; } else { dark_cluster += tSW.rgb; dark_n += 1.0; }
					if (lw >= mid) { light_cluster += tW.rgb; light_n += 1.0; } else { dark_cluster += tW.rgb; dark_n += 1.0; }
					if (lwn >= mid) { light_cluster += tWN.rgb; light_n += 1.0; } else { dark_cluster += tWN.rgb; dark_n += 1.0; }
					if (light_n > 0.0 && dark_n > 0.0) {
						light_cluster /= light_n;
						dark_cluster /= dark_n;
						vec3 target = (la >= mid) ? light_cluster : dark_cluster;
						sharpened = mix(a.rgb, target, cut_sharpening_amount * softness);
						// Soft edges keep a softer reconstruction in the final pass.
						strength *= mix(1.0, 0.4, softness);
					}
				}
			}
		}

		frag_color = vec4(sharpened, cut_pack_descriptor(cls, strength));
	}
#endif // MODE_PASS1

#if !defined(MODE_PASS1)
	// Final passes: CUT interpolation + post composition (tonemap, glow, SSAO,
	// luminance multiplier, BCS, color correction - same as post.glsl
	// MODE_DEFAULT).
	{
		vec4 color = cut_interpolate(uv_interp);

#ifdef USE_LUMINANCE_MULTIPLIER
		color = color / luminance_multiplier;
#endif

#ifdef USE_GLOW
		// Glow blending is performed before srgb_to_linear because
		// the glow texture was created from a nonlinear sRGB-encoded
		// scene, so it only makes sense to add this glow to an equally
		// nonlinear sRGB-encoded scene.

		vec4 glow = get_glow_color(uv_interp) * glow_intensity;

		// Glow always uses the screen blend mode in the Compatibility renderer:

		// Glow cannot be above 1.0 after normalizing and should be non-negative
		// to produce expected results. It is possible that glow can be negative
		// if negative lights were used in the scene.
		// We clamp to srgb_white because glow will be normalized to this range.
		// Note: srgb_white cannot be smaller than the maximum output value (1.0).
		glow.rgb = clamp(glow.rgb, 0.0, srgb_white);

		// The following is a mathematically simplified version of the above.
		color.rgb = color.rgb + glow.rgb - (color.rgb * glow.rgb / srgb_white);
#endif // USE_GLOW

		color.rgb = srgb_to_linear(color.rgb);

#if defined(USE_SOME_SSAO)
		// Putting SSAO after the conversion to linear color, though it might be better before the glow.
		color.rgb *= s4ao(uv_interp); // The USE_SSAO_X controls the number of samples.
#endif

		color.rgb = apply_tonemapping(color.rgb);

#ifdef USE_BCS
		// Apply brightness:
		// Apply to relative luminance. This ensures that the hue and saturation of
		// colors is not affected by the adjustment, but requires the multiplication
		// to be performed on linear-encoded values.
		color.rgb = color.rgb * brightness;

		color.rgb = linear_to_srgb(color.rgb);

		// Apply contrast:
		// By applying contrast to RGB values that are perceptually uniform (nonlinear),
		// the darkest values are not hard-clipped as badly, which produces a
		// higher quality contrast adjustment and maintains compatibility with
		// existing projects.
		color.rgb = mix(vec3(0.5), color.rgb, contrast);

		// Apply saturation:
		// By applying saturation adjustment to nonlinear sRGB-encoded values with
		// even weights the preceived brightness of blues are affected, but this
		// maintains compatibility with existing projects.
		color.rgb = mix(vec3(dot(vec3(1.0), color.rgb) * (1.0 / 3.0)), color.rgb, saturation);
#else
		color.rgb = linear_to_srgb(color.rgb);
#endif // USE_BCS

#ifdef USE_COLOR_CORRECTION
		color.rgb = apply_color_correction(color.rgb);
#endif

		// The final output must be opaque: the alpha of the sampled texels is
		// the edge descriptor, not opacity. An alpha of ~0 here makes the
		// result look transparent/faded wherever the viewport texture is used.
		frag_color = vec4(color.rgb, 1.0);
	}
#endif // final passes
}
