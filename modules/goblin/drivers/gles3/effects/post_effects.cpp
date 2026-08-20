/**************************************************************************/
/*  post_effects.cpp                                                      */
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

#include "post_effects.h"

#ifdef GLES3_ENABLED

#include "core/config/project_settings.h"
#include "core/math/vector2.h"
#include "drivers/gles3/storage/texture_storage.h"

using namespace GLES3;

// CUT shader state. File-scope (not a PostEffects member) so sizeof(PostEffects)
// stays identical to upstream — the class is instantiated by the upstream
// rasterizer_gles3.cpp via memnew(PostEffects) with the upstream header. A
// larger class layout would overrun that allocation (B-14 root cause).
struct Cut {
	CutShaderGLES3 shader;
	RID shader_version;
};

static Cut cut;

PostEffects *PostEffects::singleton = nullptr;

PostEffects *PostEffects::get_singleton() {
	return singleton;
}

PostEffects::PostEffects() {
	singleton = this;

	post.shader.initialize();
	post.shader_version = post.shader.version_create();
	post.shader.version_bind_shader(post.shader_version, PostShaderGLES3::MODE_DEFAULT);

	cut.shader.initialize();
	cut.shader_version = cut.shader.version_create();

	// CUT upscaler settings. Registered here (PostEffects lives as long as the
	// GL Compatibility renderer) so no core file needs to be overridden for
	// them. Values already stored in project.godot are preserved.
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/cut_blend_sharpness", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), 0.5f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/cut_edge_min_value", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), 0.05f);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "rendering/scaling_3d/cut_fast_luma"), false);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/cut_soft_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), 0.20f);
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "rendering/scaling_3d/cut_sharpening_amount", PROPERTY_HINT_RANGE, "0.0,2.0,0.01"), 0.4f);

	{ // Screen Triangle.
		glGenBuffers(1, &screen_triangle);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);

		const float qv[6] = {
			-1.0f,
			-1.0f,
			3.0f,
			-1.0f,
			-1.0f,
			3.0f,
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, qv, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		glGenVertexArrays(1, &screen_triangle_array);
		glBindVertexArray(screen_triangle_array);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);
		glVertexAttribPointer(RSE::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(RSE::ARRAY_VERTEX);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}
}

PostEffects::~PostEffects() {
	singleton = nullptr;
	glDeleteBuffers(1, &screen_triangle);
	glDeleteVertexArrays(1, &screen_triangle_array);
	post.shader.version_free(post.shader_version);
	cut.shader.version_free(cut.shader_version);
}

void PostEffects::_draw_screen_triangle() {
	glBindVertexArray(screen_triangle_array);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

bool PostEffects::post_cut(
		GLuint p_dest_framebuffer, Size2i p_dest_size, GLuint p_source_color,
		GLuint p_source_depth, bool p_ssao_enabled, int p_ssao_quality_level, float p_ssao_strength, float p_ssao_radius,
		GLuint p_cut1_color, GLuint p_cut1_fbo,
		Size2i p_source_size, RSE::ViewportScaling3DMode p_scaling_mode,
		float p_luminance_multiplier, const Glow::Level *p_glow_buffers, float p_glow_intensity,
		float p_srgb_white, uint64_t p_spec_constants) {
	uint64_t flags = p_spec_constants;
	if (p_glow_buffers != nullptr) {
		flags |= CutShaderGLES3::USE_GLOW;
	}
	if (p_ssao_enabled) {
		if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_VERY_LOW) {
			flags |= CutShaderGLES3::USE_SSAO_ABYSS;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_LOW) {
			flags |= CutShaderGLES3::USE_SSAO_LOW;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_HIGH) {
			flags |= CutShaderGLES3::USE_SSAO_HIGH;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_ULTRA) {
			flags |= CutShaderGLES3::USE_SSAO_MEGA;
		} else {
			flags |= CutShaderGLES3::USE_SSAO_MED;
		}
	}
	if (p_luminance_multiplier != 1.0) {
		flags |= CutShaderGLES3::USE_LUMINANCE_MULTIPLIER;
	}

	bool cut2 = p_scaling_mode == RSE::VIEWPORT_SCALING_3D_MODE_CUT2;

	float cut_blend_sharpness = GLOBAL_GET("rendering/scaling_3d/cut_blend_sharpness");
	float cut_edge_min_value = GLOBAL_GET("rendering/scaling_3d/cut_edge_min_value");
	float cut_fast_luma = bool(GLOBAL_GET("rendering/scaling_3d/cut_fast_luma")) ? 1.0f : 0.0f;
	float cut_soft_threshold = GLOBAL_GET("rendering/scaling_3d/cut_soft_threshold");
	float cut_sharpening_amount = GLOBAL_GET("rendering/scaling_3d/cut_sharpening_amount");

	Vector2 source_size_vec2(p_source_size.x, p_source_size.y);

	// Every CUT read uses point (nearest) sampling - the algorithm computes its
	// own interpolation weights, hardware bilinear would double-filter.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, p_source_color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// The intermediate passes write the edge descriptor into the alpha channel;
	// the scene render may leave the alpha write masked off.
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// Pass 1 (CUT2): soft-edge sharpening + edge descriptors at input size.
	if (cut2) {
		glBindFramebuffer(GL_FRAMEBUFFER, p_cut1_fbo);
		glViewport(0, 0, p_source_size.x, p_source_size.y);

		bool success = cut.shader.version_bind_shader(cut.shader_version, CutShaderGLES3::MODE_PASS1);
		if (!success) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
			return false;
		}

		cut.shader.version_set_uniform(CutShaderGLES3::SOURCE_SIZE, source_size_vec2, cut.shader_version, CutShaderGLES3::MODE_PASS1);
		cut.shader.version_set_uniform(CutShaderGLES3::CUT_EDGE_MIN_VALUE, cut_edge_min_value, cut.shader_version, CutShaderGLES3::MODE_PASS1);
		cut.shader.version_set_uniform(CutShaderGLES3::CUT_FAST_LUMA, cut_fast_luma, cut.shader_version, CutShaderGLES3::MODE_PASS1);
		cut.shader.version_set_uniform(CutShaderGLES3::CUT_SOFT_THRESHOLD, cut_soft_threshold, cut.shader_version, CutShaderGLES3::MODE_PASS1);
		cut.shader.version_set_uniform(CutShaderGLES3::CUT_SHARPENING_AMOUNT, cut_sharpening_amount, cut.shader_version, CutShaderGLES3::MODE_PASS1);

		_draw_screen_triangle();
	}

	// Final pass: CUT interpolation + tonemap/glow/SSAO composition.
	glBindFramebuffer(GL_FRAMEBUFFER, p_dest_framebuffer);
	glViewport(0, 0, p_dest_size.x, p_dest_size.y);

	GLuint final_color = cut2 ? p_cut1_color : p_source_color;
	glBindTexture(GL_TEXTURE_2D, final_color);

	CutShaderGLES3::ShaderVariant mode = cut2 ? CutShaderGLES3::MODE_FINAL_SOFT : CutShaderGLES3::MODE_FINAL;

	bool success = cut.shader.version_bind_shader(cut.shader_version, mode, flags);
	if (!success) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
		return false;
	}

	if (p_ssao_enabled) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, p_source_depth);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		cut.shader.version_set_uniform(CutShaderGLES3::SSAO_INTENSITY, p_ssao_strength, cut.shader_version, mode, flags);
		cut.shader.version_set_uniform(CutShaderGLES3::SSAO_RADIUS_FRAC, p_ssao_radius, cut.shader_version, mode, flags);
		cut.shader.version_set_uniform(CutShaderGLES3::SSAO_PRN_UV,
				p_source_size.x * 1.087f * ((1.0f + sqrt(5.0f)) / 2.0f),
				p_source_size.y * 1.087f * ((9.0f + sqrt(221.0f)) / 10.0f),
				cut.shader_version, mode, flags);
	}

	if (p_glow_buffers != nullptr) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, p_glow_buffers[0].color);

		cut.shader.version_set_uniform(CutShaderGLES3::PIXEL_SIZE, 1.0 / p_source_size.x, 1.0 / p_source_size.y, cut.shader_version, mode, flags);
		cut.shader.version_set_uniform(CutShaderGLES3::GLOW_INTENSITY, p_glow_intensity, cut.shader_version, mode, flags);
		cut.shader.version_set_uniform(CutShaderGLES3::SRGB_WHITE, p_srgb_white, cut.shader_version, mode, flags);
	}

	cut.shader.version_set_uniform(CutShaderGLES3::SOURCE_SIZE, source_size_vec2, cut.shader_version, mode, flags);
	cut.shader.version_set_uniform(CutShaderGLES3::CUT_BLEND_SHARPNESS, cut_blend_sharpness, cut.shader_version, mode, flags);
	cut.shader.version_set_uniform(CutShaderGLES3::CUT_EDGE_MIN_VALUE, cut_edge_min_value, cut.shader_version, mode, flags);
	cut.shader.version_set_uniform(CutShaderGLES3::CUT_FAST_LUMA, cut_fast_luma, cut.shader_version, mode, flags);
	cut.shader.version_set_uniform(CutShaderGLES3::LUMINANCE_MULTIPLIER, p_luminance_multiplier, cut.shader_version, mode, flags);

	_draw_screen_triangle();

	// Reset state.
	if (p_glow_buffers != nullptr) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	if (p_ssao_enabled) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);

	return true;
}

void PostEffects::post_copy(
		GLuint p_dest_framebuffer, Size2i p_dest_size, GLuint p_source_color,
		GLuint p_source_depth, bool p_ssao_enabled, int p_ssao_quality_level, float p_ssao_strength, float p_ssao_radius,
		Size2i p_source_size, float p_luminance_multiplier, const Glow::Level *p_glow_buffers, float p_glow_intensity,
		float p_srgb_white, uint32_t p_view, bool p_use_multiview, uint64_t p_spec_constants,
		RSE::ViewportScaling3DMode p_scaling_mode, GLuint p_cut1_color, GLuint p_cut1_fbo) {
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, p_dest_framebuffer);
	glViewport(0, 0, p_dest_size.x, p_dest_size.y);

	if (p_scaling_mode == RSE::VIEWPORT_SCALING_3D_MODE_CUT1 || p_scaling_mode == RSE::VIEWPORT_SCALING_3D_MODE_CUT2) {
		if (post_cut(p_dest_framebuffer, p_dest_size, p_source_color,
					p_source_depth, p_ssao_enabled, p_ssao_quality_level, p_ssao_strength, p_ssao_radius,
					p_cut1_color, p_cut1_fbo,
					p_source_size, p_scaling_mode, p_luminance_multiplier, p_glow_buffers, p_glow_intensity,
					p_srgb_white, p_spec_constants)) {
			return;
		}
		// Fall through to the bilinear path if the CUT shader failed to bind.
	}

	bool p_bilinear_filtering = p_scaling_mode != RSE::VIEWPORT_SCALING_3D_MODE_NEAREST;

	PostShaderGLES3::ShaderVariant mode = PostShaderGLES3::MODE_DEFAULT;
	uint64_t flags = p_spec_constants;
	if (p_use_multiview) {
		flags |= PostShaderGLES3::USE_MULTIVIEW;
	}
	if (p_glow_buffers != nullptr) {
		flags |= PostShaderGLES3::USE_GLOW;
	}
	if (p_ssao_enabled) {
		if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_VERY_LOW) {
			flags |= PostShaderGLES3::USE_SSAO_ABYSS;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_LOW) {
			flags |= PostShaderGLES3::USE_SSAO_LOW;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_HIGH) {
			flags |= PostShaderGLES3::USE_SSAO_HIGH;
		} else if (p_ssao_quality_level == RSE::ENV_SSAO_QUALITY_ULTRA) {
			flags |= PostShaderGLES3::USE_SSAO_MEGA;
		} else {
			flags |= PostShaderGLES3::USE_SSAO_MED;
		}
	}
	if (p_luminance_multiplier != 1.0) {
		flags |= PostShaderGLES3::USE_LUMINANCE_MULTIPLIER;
	}

	bool success = post.shader.version_bind_shader(post.shader_version, mode, flags);
	if (!success) {
		return;
	}

	GLenum texture_target = p_use_multiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture_target, p_source_color);

	glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, p_bilinear_filtering ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, p_bilinear_filtering ? GL_LINEAR : GL_NEAREST);

	if (p_ssao_enabled) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(texture_target, p_source_depth);
		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST); // Thanks to mrjustaguy!
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		post.shader.version_set_uniform(PostShaderGLES3::SSAO_INTENSITY, p_ssao_strength, post.shader_version, mode, flags);
		post.shader.version_set_uniform(PostShaderGLES3::SSAO_RADIUS_FRAC, p_ssao_radius, post.shader_version, mode, flags);
		post.shader.version_set_uniform(PostShaderGLES3::SSAO_PRN_UV, // This converts the UV coordinate into a pseudo-random number.
				p_source_size.x * 1.087f * ((1.0f + sqrt(5.0f)) / 2.0f),
				p_source_size.y * 1.087f * ((9.0f + sqrt(221.0f)) / 10.0f),
				post.shader_version, mode, flags);
	}

	if (p_glow_buffers != nullptr) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, p_glow_buffers[0].color);

		post.shader.version_set_uniform(PostShaderGLES3::PIXEL_SIZE, 1.0 / p_source_size.x, 1.0 / p_source_size.y, post.shader_version, mode, flags);
		post.shader.version_set_uniform(PostShaderGLES3::GLOW_INTENSITY, p_glow_intensity, post.shader_version, mode, flags);
		post.shader.version_set_uniform(PostShaderGLES3::SRGB_WHITE, p_srgb_white, post.shader_version, mode, flags);
	}

	post.shader.version_set_uniform(PostShaderGLES3::VIEW, float(p_view), post.shader_version, mode, flags);
	post.shader.version_set_uniform(PostShaderGLES3::LUMINANCE_MULTIPLIER, p_luminance_multiplier, post.shader_version, mode, flags);

	_draw_screen_triangle();

	// Reset state
	if (p_glow_buffers != nullptr) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	if (p_ssao_enabled) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(texture_target, 0);
	}

	// Return back to nearest
	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(texture_target, 0);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, GLES3::TextureStorage::system_fbo);
}

#endif // GLES3_ENABLED
