/**************************************************************************/
/*  cut.glsl.gen.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

/* THIS FILE IS GENERATED. EDITS WILL BE LOST. */

#pragma once

#include "drivers/gles3/shader_gles3.h"

class CutShaderGLES3 : public ShaderGLES3 {
public:
	enum Uniforms {
		VIEW,
		LUMINANCE_MULTIPLIER,
		PIXEL_SIZE,
		GLOW_INTENSITY,
		SRGB_WHITE,
		SSAO_INTENSITY,
		SSAO_RADIUS_FRAC,
		SSAO_PRN_UV,
		SOURCE_SIZE,
		CUT_BLEND_SHARPNESS,
		CUT_EDGE_MIN_VALUE,
		CUT_FAST_LUMA,
		CUT_SOFT_THRESHOLD,
		CUT_SHARPENING_AMOUNT,
	};

	enum ShaderVariant {
		MODE_FINAL,
		MODE_PASS1,
		MODE_FINAL_SOFT,
	};

	enum Specializations {
		USE_GLOW = 1,
		USE_LUMINANCE_MULTIPLIER = 2,
		USE_BCS = 4,
		USE_COLOR_CORRECTION = 8,
		USE_1D_LUT = 16,
		USE_SSAO_ABYSS = 32,
		USE_SSAO_LOW = 64,
		USE_SSAO_MED = 128,
		USE_SSAO_HIGH = 256,
		USE_SSAO_MEGA = 512,
	};

	_FORCE_INLINE_ bool version_bind_shader(RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		return _version_bind_shader(p_version, p_variant, p_specialization);
	}

	_FORCE_INLINE_ int version_get_uniform(Uniforms p_uniform, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		return _version_get_uniform(p_uniform, p_version, p_variant, p_specialization);
	}

	/* clang-format off */
#define TRY_GET_UNIFORM(var_name) int var_name = version_get_uniform(p_uniform, p_version, p_variant, p_specialization); if (var_name < 0) return
	/* clang-format on */

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1f(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, double p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1f(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint8_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1ui(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int8_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1i(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint16_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1ui(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int16_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1i(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint32_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1ui(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int32_t p_value, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform1i(uniform_location, p_value);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Color &p_color, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLfloat col[4] = { p_color.r, p_color.g, p_color.b, p_color.a };
		glUniform4fv(uniform_location, 1, col);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector2 &p_vec2, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLfloat vec2[2] = { float(p_vec2.x), float(p_vec2.y) };
		glUniform2fv(uniform_location, 1, vec2);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Size2i &p_vec2, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLint vec2[2] = { GLint(p_vec2.x), GLint(p_vec2.y) };
		glUniform2iv(uniform_location, 1, vec2);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector3 &p_vec3, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLfloat vec3[3] = { float(p_vec3.x), float(p_vec3.y), float(p_vec3.z) };
		glUniform3fv(uniform_location, 1, vec3);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector4 &p_vec4, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLfloat vec4[4] = { float(p_vec4.x), float(p_vec4.y), float(p_vec4.z), float(p_vec4.w) };
		glUniform4fv(uniform_location, 1, vec4);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform2f(uniform_location, p_a, p_b);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform3f(uniform_location, p_a, p_b, p_c);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, float p_d, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		glUniform4f(uniform_location, p_a, p_b, p_c, p_d);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Transform3D &p_transform, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		const Transform3D &tr = p_transform;

		GLfloat matrix[16] = { /* build a 16x16 matrix */
			(GLfloat)tr.basis.rows[0][0],
			(GLfloat)tr.basis.rows[1][0],
			(GLfloat)tr.basis.rows[2][0],
			(GLfloat)0,
			(GLfloat)tr.basis.rows[0][1],
			(GLfloat)tr.basis.rows[1][1],
			(GLfloat)tr.basis.rows[2][1],
			(GLfloat)0,
			(GLfloat)tr.basis.rows[0][2],
			(GLfloat)tr.basis.rows[1][2],
			(GLfloat)tr.basis.rows[2][2],
			(GLfloat)0,
			(GLfloat)tr.origin.x,
			(GLfloat)tr.origin.y,
			(GLfloat)tr.origin.z,
			(GLfloat)1
		};

		glUniformMatrix4fv(uniform_location, 1, false, matrix);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Transform2D &p_transform, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		const Transform2D &tr = p_transform;

		GLfloat matrix[16] = { /* build a 16x16 matrix */
			(GLfloat)tr.columns[0][0],
			(GLfloat)tr.columns[0][1],
			(GLfloat)0,
			(GLfloat)0,
			(GLfloat)tr.columns[1][0],
			(GLfloat)tr.columns[1][1],
			(GLfloat)0,
			(GLfloat)0,
			(GLfloat)0,
			(GLfloat)0,
			(GLfloat)1,
			(GLfloat)0,
			(GLfloat)tr.columns[2][0],
			(GLfloat)tr.columns[2][1],
			(GLfloat)0,
			(GLfloat)1
		};

		glUniformMatrix4fv(uniform_location, 1, false, matrix);
	}

	_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Projection &p_matrix, RID p_version, ShaderVariant p_variant, uint64_t p_specialization = 0) {
		TRY_GET_UNIFORM(uniform_location);
		GLfloat matrix[16];

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				matrix[i * 4 + j] = p_matrix.columns[i][j];
			}
		}

		glUniformMatrix4fv(uniform_location, 1, false, matrix);
	}

#undef TRY_GET_UNIFORM

protected:
	virtual void _init() override {
		static const char *_uniform_strings[] = {
			"view",
			"luminance_multiplier",
			"pixel_size",
			"glow_intensity",
			"srgb_white",
			"ssao_intensity",
			"ssao_radius_frac",
			"ssao_prn_UV",
			"source_size",
			"cut_blend_sharpness",
			"cut_edge_min_value",
			"cut_fast_luma",
			"cut_soft_threshold",
			"cut_sharpening_amount"
		};
		static const char *_variant_defines[] = {
			"#define MODE_FINAL",
			"#define MODE_PASS1",
			"#define MODE_FINAL_SOFT",
		};
		static TexUnitPair _texunit_pairs[] = {
			{ "source_color", 0 },
			{ "glow_color", 1 },
			{ "source_color_correction", 2 },
			{ "depth_buffer", 3 },
		};
		static UBOPair _ubo_pairs[] = {
			{ "TonemapData", 0 },
		};
		static Specialization _spec_pairs[] = {
			{ "USE_GLOW", false },
			{ "USE_LUMINANCE_MULTIPLIER", false },
			{ "USE_BCS", false },
			{ "USE_COLOR_CORRECTION", false },
			{ "USE_1D_LUT", false },
			{ "USE_SSAO_ABYSS", false },
			{ "USE_SSAO_LOW", false },
			{ "USE_SSAO_MED", false },
			{ "USE_SSAO_HIGH", false },
			{ "USE_SSAO_MEGA", false },
		};
		static const Feedback *_feedbacks = nullptr;
		static const char _vertex_code[] = {
R"<!>(layout(location = 0) in vec2 vertex_attrib;

/* clang-format on */

out vec2 uv_interp;

void main() {
	uv_interp = vertex_attrib * 0.5 + 0.5;
	gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

/* clang-format off */
)<!>"
		};

		static const char _fragment_code[] = {
(R"<!>(/* clang-format on */

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

layout(std140) uniform TonemapData { //ubo:0
	float exposure;
	int tonemapper;
	int pad;
	int pad2;
	vec4 tonemapper_params;
	float brightness;
	float contrast;
	float saturation;
	int pad3;
};

// This approximation expects non-negative input; negative input is undefined behavior.
vec3 linear_to_srgb(vec3 color) {
	//const vec3 a = vec3(0.055f);
	//return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

// This approximation expects non-negative input; negative input behaves poorly.
vec3 srgb_to_linear(vec3 color) {
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

#ifdef APPLY_TONEMAPPING

// Based on Reinhard's extended formula, see equation 4 in https://doi.org/cjbgrt
vec3 tonemap_reinhard(vec3 color) {
	float white_squared = tonemapper_params.x;
	vec3 white_squared_color = white_squared * color;
	// Equivalent to color * (1 + color / white_squared) / (1 + color)
	return (white_squared_color + color * color) / (white_squared_color + white_squared);
}

vec3 tonemap_filmic(vec3 color) {
	// These constants must match the those in the C++ code that calculates the parameters.
	// exposure_bias: Input scale (color *= bias, env->white *= bias) to make the brightness consistent with other tonemappers.
	// Also useful to scale the input to the range that the tonemapper is designed for (some require very high input values).
	// Has no effect on the curve's general shape or visual properties.
	const float exposure_bias = 2.0f;
	const float A = 0.22f * exposure_bias * exposure_bias; // bias baked into constants for performance
	const float B = 0.30f * exposure_bias;
	const float C = 0.10f;
	const float D = 0.20f;
	const float E = 0.01f;
	const float F = 0.30f;

	vec3 color_tonemapped = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;

	return color_tonemapped / tonemapper_params.x;
}

// Adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// (MIT License).
vec3 tonemap_aces(vec3 color) {
	// These constants must match the those in the C++ code that calculates the parameters.
	const float exposure_bias = 1.8f;
	const float A = 0.0245786f;
	const float B = 0.000090537f;
	const float C = 0.983729f;
	const float D = 0.432951f;
	const float E = 0.238081f;

	// Exposure bias baked into transform to save shader instructions. Equivalent to `color *= exposure_bias`
	const mat3 rgb_to_rrt = mat3(
			vec3(0.59719f * exposure_bias, 0.35458f * exposure_bias, 0.04823f * exposure_bias),
			vec3(0.07600f * exposure_bias, 0.90834f * exposure_bias, 0.01566f * exposure_bias),
			vec3(0.02840f * exposure_bias, 0.13383f * exposure_bias, 0.83777f * exposure_bias));

	const mat3 odt_to_rgb = mat3(
			vec3(1.60475f, -0.53108f, -0.07367f),
			vec3(-0.10208f, 1.10813f, -0.00605f),
			vec3(-0.00327f, -0.07276f, 1.07602f));

	color *= rgb_to_rrt;
	vec3 color_tonemapped = (color * (color + A) - B) / (color * (C * color + D) + E);
	color_tonemapped *= odt_to_rgb;

	return color_tonemapped / tonemapper_params.x;
}

// allenwp tonemapping curve; developed for use in the Godot game engine.
// Source and details: https://allenwp.com/blog/2025/05/29/allenwp-tonemapping-curve/
// Input must be a non-negative linear scene value.
vec3 allenwp_curve(vec3 x) {
	const float output_max_value = 1.0; // SDR always has an output_max_value of 1.0

	// These constants must match the those in the C++ code that calculates the parameters.
	// 18% "middle gray" is perceptually 50% of the brightness of reference white.
	const float awp_crossover_point = 0.18;
	// When output_max_value and/or awp_crossover_point are no longer constant,
	// awp_shoulder_max can be calculated on the CPU and passed in as tonemap_e.
	const float awp_shoulder_max = output_max_value - awp_crossover_point;

	float awp_contrast = tonemapper_params.x;
	float awp_toe_a = tonemapper_params.y;
	float awp_slope = tonemapper_params.z;
	float awp_w = tonemapper_params.w;

	// Reinhard-like shoulder:
	vec3 s = x - awp_crossover_point;
	vec3 slope_s = awp_slope * s;
	s = slope_s * (1.0 + s / awp_w) / (1.0 + (slope_s / awp_shoulder_max));
	s += awp_crossover_point;

	// Sigmoid power function toe:
	vec3 t = pow(x, vec3(awp_contrast));
	t = t / (t + awp_toe_a);

	return mix(s, t, lessThan(x, vec3(awp_crossover_point)));
}

// This is an approximation and simplification of EaryChow's AgX implementation that is used by Blender.
// This code is based off of the script that generates the AgX_Base_sRGB.cube LUT that Blender uses.
// Source: https://github.com/EaryChow/AgX_LUT_Gen/blob/main/AgXBasesRGB.py
// Colorspace transformation source: https://www.colour-science.org:8010/apps/rgb_colourspace_transformation_matrix
vec3 tonemap_agx(vec3 color) {
	// Input color should be non-negative!
	// Large negative values in one channel and large positive values in other
	// channels can result in a colour that appears darker and more saturated than
	// desired after passing it through the inset matrix. For this reason, it is
	// best to prevent negative input values.
	// This is done before the Rec. 2020 transform to allow the Rec. 2020
	// transform to be combined with the AgX inset matrix. This results in a loss
	// of color information that could be correctly interpreted within the
	// Rec. 2020 color space as positive RGB values, but is often not worth
	// the performance cost of an additional matrix multiplication.
	//
	// Additionally, this AgX configuration was created subjectively based on
	// output appearance in the Rec. 709 color gamut, so it is possible that these
	// matrices will not perform well with non-Rec. 709 output (more testing with
	// future wide-gamut displays is be needed).
	// See this comment from the author on the decisions made to create the matrices:
	// https://github.com/godotengine/godot-proposals/issues/12317#issuecomment-2835824250

	// Combined Rec. 709 to Rec. 2020 and Blender AgX inset matrices:
	const mat3 rec709_to_rec2020_agx_inset_matrix = mat3(
			0.544814746488245, 0.140416948464053, 0.0888104196149096,
			0.373787398372697, 0.754137554567394, 0.178871756420858,
			0.0813978551390581, 0.105445496968552, 0.732317823964232);

	// Combined inverse AgX outset matrix and Rec. 2020 to Rec. 709 matrices.
	const mat3 agx_outset_rec2020_to_rec709_matrix = mat3(
			1.96488741169489, -0.299313364904742, -0.164352742528393,
			-0.855988495690215, 1.32639796461980, -0.238183969428088,
			-0.108898916004672, -0.0270845997150571, 1.40253671195648);

	const float output_max_value = 1.0; // SDR always has an output_max_value of 1.0

	// Apply inset matrix.
	color = rec709_to_rec2020_agx_inset_matrix * color;

	// Use the allenwp tonemapping curve to match the Blender AgX curve while
	// providing stability across all variable dyanimc range (SDR, HDR, EDR).
	color = allenwp_curve(color);

	// Clipping to output_max_value is required to address a cyan colour that occurs
	// with very bright inputs.
	color = min(vec3(output_max_value), color);

	// Apply outset to make the result more chroma-laden and then go back to Rec. 709.
	color = agx_outset_rec2020_to_rec709_matrix * color;

	// Blender's lusRGB.compensate_low_side is too complex for this shader, so
	// simply return the color, even if it has negative components. These negative
	// components may be useful for subsequent color adjustments.
	return color;
}

#define TONEMAPPER_LINEAR 0
#define TONEMAPPER_REINHARD 1
#define TONEMAPPER_FILMIC 2
#define TONEMAPPER_ACES 3
#define TONEMAPPER_AGX 4

vec3 apply_tonemapping(vec3 color) { // inputs are LINEAR
	if (tonemapper == TONEMAPPER_LINEAR) {
		return color;
	}

	// Ensure color values passed to tonemappers are positive.
	// They can be negative in the case of negative lights, which leads to undesired behavior.
	color = max(vec3(0.0), color);

	if (tonemapper == TONEMAPPER_REINHARD) {
		return tonemap_reinhard(color);
	} else if (tonemapper == TONEMAPPER_FILMIC) {
		return tonemap_filmic(color);
	} else if (tonemapper == TONEMAPPER_ACES) {
		return tonemap_aces(color);
	} else { // TONEMAPPER_AGX
		return tonemap_agx(color);
	}
}

#endif // APPLY_TONEMAPPING

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
// S4AO (Stupid Simple Screen Space Ambient Occlusion) - Jonathan Dummer (O1S)
// This micro version uses only 3 depth samples, the midpoint and a randomly-rotated, balanced pair.

const mediump float ssao_falloff_frac = 0.25;
// Perform the SSAO.
float s4ao(vec2 UV) {
#ifdef USE_MULTIVIEW
	mediump float depth = texture(depth_buffer_array, vec3(UV, view)).r;
#else
	mediump float depth = texture(depth_buffer, UV).r;
#endif
	mediump float inv_falloff = 1.0f / max(1e-4f, depth * ssao_falloff_frac);
	// Random 2D rotation per pixel (0..1 -> parabola approximating a 180 deg arc)
	mediump float r01 = fract(dot(UV, ssao_prn_UV));
	mediump vec2 duv = vec2(r01 - 0.5f, 2.0f * (r01 - r01 * r01)) * (2.0f * depth * ssao_radius_frac); // 180 degrees.
	// Grab the samples and determine the occlusion.
	mediump float occlusion = 0.0f;
	for (int s = 0; s < 2; ++s) {
#ifdef USE_MULTIVIEW
		mediump float dz = texture(depth_buffer_array, vec3(UV + duv, view)).r - depth;
#else
		mediump float dz = texture(depth_buffer, UV + duv).r - depth;
#endif
		// How 'directly overhead' is it?  Factor in the falloff depth.
		occlusion += normalize(vec3(duv, dz)).z * mix(1.0f, 0.0f, dz * inv_falloff);
		// Mirror the next sample.
		duv = -duv;
	}
	// Adjust the occlusion for intensity, and # samples.
	occlusion = 1.0f - clamp(occlusion * 0.5f * ssao_intensity, 0.0f, 1.0f);
	return occlusion * occlusion;
}
#elif defined(USE_SSAO_HIGH) || defined(USE_SSAO_MEGA)
// Use the rings version for the higher qualities.
// S4AO (Stupid Simple Screen Space Ambient Occlusion) - Jonathan Dummer (O1S)
// The mega version uses N concentric rings of samples.

#if defined(USE_SSAO_MEGA)
const int rings = 4; // Start with the outer ring.
const int samps[] = int[](24, 18, 12, 6); // ( 9, 6, 3 ) is a minimum, but I want better.
#else
const int rings = 3; // Start with the outer ring.
const int samps[] = int[](15, 10, 5, 1); // ( 9, 6, 3 ) is a minimum, but I want better.
#endif
const float average_samples = 1.0 / float(samps[0] + samps[1] * int(rings > 1) + samps[2] * int(rings > 2) + samps[3] * int(rings > 3));
const float ssao_falloff_frac = 0.25;
// Perform the SSAO.
float s4ao(vec2 UV) {
#ifdef USE_MULTIVIEW
	float depth = texture(depth_buffer_array, vec3(UV, view)).r;
#else
	float depth = texture(depth_buffer, UV).r;
#endif
	float inv_falloff = 1.0f / max(1e-4f, depth * ssao_falloff_frac);
	// Random 2D rotation per pixel (0..1 -> parabola approximating a 180 deg arc)
	float r01 = fract(dot(UV, ssao_prn_UV));
	vec2 rcos = vec2(r01 - 0.5f, 2.0f * (r01 - r01 * r01)) * (2.0f * depth * ssao_radius_frac); // 180 degrees.
	vec2 rsin = rcos.yx * vec2(-1, 1); // Perpendicular to the random cosine vector.
	// Grab the samples and determine the occlusion.
	float occlusion = 0.0f;
	float ring_shrink = 0.75f; // Shrink every ring.
	for (int r = 0; r < rings; ++r) {
		float dt = (6.283185307f) / float(samps[r]);
		float t = float(r & 1) * 0.5f * dt;
		for (int s = 0; s < samps[r]; ++s) {
			vec2 duv = cos(t) * rcos + sin(t) * rsin;
#ifdef USE_MULTIVIEW
			float dz = texture(depth_buffer_array, vec3(UV + duv, view)).r - depth;
#else
			float dz = texture(depth_buffer, UV + duv).r - depth;
#endif
			// How 'directly overhead' is it?  Factor in the falloff depth.
			occlusion += normalize(vec3(duv, dz)).z * smoothstep(1.0f, 0.0f, dz * inv_falloff);
			t += dt;
		}
		// The next ring will be smaller.
		rcos *= ring_shrink;
		rsin *= ring_shrink;
	}
	// Adjust the occlusion for intensity, and # samples.
	occlusion *= ssao_intensity * average_samples;
	occlusion = 1.0f - clamp(occlusion, 0.0f, 1.0f);
	return occlusion * occlusion;
}
#else
// Use the more generic NxN grid version.
// S4AO (Stupid Simple Screen Space Ambient Occlusion) - Jonathan Dummer (O1S)
)<!>" R"<!>(
// The sample_width should be even, else the midpoint is at UV.
// Takes sample_width^2 samples in a grid, with the corners notched.
#if defined(USE_SSAO_LOW)
const int sample_width = 2;
#elif defined(USE_SSAO_HIGH)
const int sample_width = 6;
#else
const int sample_width = 4;
#endif
const int notch_01 = int(sample_width > 3); // Set to 1 to skip the corner samples, 0 to include them.
const float sample_mid = (float(sample_width) - 1.0) * 0.50001; // Can't be exactly 0.5 in case sample_width is odd.
#if defined(USE_SSAO_LOW)
const float inv_half_width = 1.0 / sample_mid; // The 2x2 sampling looks wider as all samples are at radius.
#else
const float inv_half_width = 1.7 / sample_mid; // Bake in the 1.7 scale for the random rotation.
#endif
const float average_samples = 1.0 / float(sample_width * sample_width - 4 * notch_01); //  1 / number_of_samples
const float ssao_falloff_frac = 0.25;
// Perform the SSAO.
float s4ao(vec2 UV) {
#ifdef USE_MULTIVIEW
	float depth = texture(depth_buffer_array, vec3(UV, view)).r;
#else
	float depth = texture(depth_buffer, UV).r;
#endif
	float radius = max(1e-4f, depth * ssao_radius_frac);
	float inv_falloff = 1.0f / max(1e-4f, depth * ssao_falloff_frac);
	// Random 2D rotation per pixel (+/-45 deg, with 0 having a lower probability).
	// The random cosine vector is vec2( 0.5, -0.5 to +0.5 ) and *1.7 makes the average length ~ 1.
	vec2 rcos = (inv_half_width * radius) * vec2(0.5f, fract(dot(UV, ssao_prn_UV)) - 0.5f);
	vec2 rsin = rcos.yx * vec2(-1, 1); // Perpendicular to the random cosine vector.
	// Grab the samples and determine the occlusion.
	float occlusion = 0.0f;
	vec2 base_duv = -sample_mid * rsin;
	for (int j = sample_width; --j >= 0;) {
#if defined(USE_SSAO_LOW)
		// Low quality uses 2x2 samples, no notching.
		vec2 duv = -sample_mid * rcos + base_duv;
		for (int i = sample_width; --i >= 0;) {
#else
		//	Will uses 4x4 or 6x6 samples, with the corners notched out.
		int o = /*notch_01 &*/ int((j <= 0) || (j >= (sample_width - 1))); // Notch corners of the grid.
		vec2 duv = (float(o) - sample_mid) * rcos + base_duv;
		for (int i = sample_width - o - o; --i >= 0;) {
#endif
#ifdef USE_MULTIVIEW
			float dz = texture(depth_buffer_array, vec3(UV + duv, view)).r - depth;
#else
			float dz = texture(depth_buffer, UV + duv).r - depth;
#endif
			float validity = smoothstep(1.0f, 0.0f, dz * inv_falloff);
			occlusion += normalize(vec3(duv, dz)).z * validity; // How 'directly overhead' is it?
			duv += rcos; // March along the rcos direction with i.
		}
		base_duv += rsin; // March along the rsin direction with j.
	}
	// Adjust the occlusion for intensity, and # samples.
	occlusion *= ssao_intensity * average_samples;
	occlusion = clamp(1.0f - occlusion, 0.0f, 1.0f);
	return occlusion * occlusion;
}
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
)<!>")
		};

		_setup(_vertex_code, _fragment_code, "CutShaderGLES3",
				14, _uniform_strings, 1, _ubo_pairs,
				0, _feedbacks, 4, _texunit_pairs,
				10, _spec_pairs, 3, _variant_defines);
	}
};
