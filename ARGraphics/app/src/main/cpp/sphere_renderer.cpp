
/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sphere_renderer.h"

#include "util.h"

    namespace {
        const glm::vec4 kLightDirection(0.0f, 1.0f, 0.0f, 0.0f);
        constexpr char kVertexShaderFilename[] = "shaders/ar_object.vert";
        constexpr char kFragmentShaderFilename[] = "shaders/ar_object.frag";
        constexpr char kUseDepthForOcclusionShaderFlag[] = "USE_DEPTH_FOR_OCCLUSION";
    }  // namespace

    void ObjRenderer::InitializeGlContent(AAssetManager* asset_manager,
                                          const std::string& obj_file_name) {
        compileAndLoadShaderProgram(asset_manager);
        position_attrib_ = glGetAttribLocation(shader_program_, "a_Position");
        tex_coord_attrib_ = glGetAttribLocation(shader_program_, "a_TexCoord");
        normal_attrib_ = glGetAttribLocation(shader_program_, "a_Normal");



        glBindTexture(GL_TEXTURE_2D, 0);

        LoadObjFile(obj_file_name, asset_manager, &vertices_, &normals_, &uvs_,
                          &indices_);
        if (normals_.empty()) {

            // if no normals, generate them ourselves
            normals_.resize(vertices_.size());
            std::fill(normals_.begin(), normals_.end(), 0);
            for (int i = 0; i < indices_.size(); i+=3) {
                auto a = glm::make_vec3(vertices_.data() + (3*indices_[i]));
                auto b = glm::make_vec3(vertices_.data() + (3*indices_[i+1]));
                auto c = glm::make_vec3(vertices_.data() + (3*indices_[i+2]));
                // (a-b) x (a-c)
                auto normal = glm::cross(a-c, a-b);
                auto add_vec3 = [&] (int index) {
                    *(normals_.data() + (3*index) + 0) += normal.x;
                    *(normals_.data() + (3*index) + 1) += normal.y;
                    *(normals_.data() + (3*index) + 2) += normal.z;
                };
                for (int j = 0; j < 3; ++j) {
                    add_vec3(indices_[i+j]);
                }
            }
            // normalize all vertex normals
            for (int i = 0; i < normals_.size(); i+=3) {
                auto n = glm::make_vec3(normals_.data() + i);
                n = glm::normalize(n);
                *(normals_.data() + i + 0) = n.x;
                *(normals_.data() + i + 1) = n.y;
                *(normals_.data() + i + 2) = n.z;
            }
        }

        CheckGlError("obj_renderer::InitializeGlContent()");
    }

    void ObjRenderer::setUseDepthForOcclusion(AAssetManager* asset_manager,
                                              bool use_depth_for_occlusion) {
        if (use_depth_for_occlusion_ == use_depth_for_occlusion) {
            return;  // No change, does nothing.
        }

        // Toggles the occlusion rendering mode and recompiles the shader.
        use_depth_for_occlusion_ = use_depth_for_occlusion;
        compileAndLoadShaderProgram(asset_manager);
    }

    void ObjRenderer::compileAndLoadShaderProgram(AAssetManager* asset_manager) {
        // Compiles and loads the shader program based on the selected mode.
        std::map<std::string, int> define_values_map;
        define_values_map[kUseDepthForOcclusionShaderFlag] =
                use_depth_for_occlusion_ ? 1 : 0;

        shader_program_ =
                CreateProgram(kVertexShaderFilename, kFragmentShaderFilename,
                                    asset_manager, define_values_map);
        if (!shader_program_) {
            LOGE("Could not create program.");
        }

        mvp_mat_uniform_ =
                glGetUniformLocation(shader_program_, "u_ModelViewProjection");
        mv_mat_uniform_ = glGetUniformLocation(shader_program_, "u_ModelView");
        texture_uniform_ = glGetUniformLocation(shader_program_, "u_Texture");

        lighting_param_uniform_ =
                glGetUniformLocation(shader_program_, "u_LightingParameters");
        material_param_uniform_ =
                glGetUniformLocation(shader_program_, "u_MaterialParameters");
        color_correction_param_uniform_ =
                glGetUniformLocation(shader_program_, "u_ColorCorrectionParameters");
        color_uniform_ = glGetUniformLocation(shader_program_, "u_ObjColor");

        // Occlusion Uniforms.
        if (use_depth_for_occlusion_) {
            depth_texture_uniform_ =
                    glGetUniformLocation(shader_program_, "u_DepthTexture");
            depth_uv_transform_uniform_ =
                    glGetUniformLocation(shader_program_, "u_DepthUvTransform");
            depth_aspect_ratio_uniform_ =
                    glGetUniformLocation(shader_program_, "u_DepthAspectRatio");
        }
    }

    void ObjRenderer::SetMaterialProperty(float ambient, float diffuse,
                                          float specular, float specular_power) {
        ambient_ = ambient;
        diffuse_ = diffuse;
        specular_ = specular;
        specular_power_ = specular_power;
    }

    void ObjRenderer::Draw(const glm::mat4& projection_mat,
                           const glm::mat4& view_mat, const glm::mat4& model_mat,
                           const float* color_correction4,
                           const float* object_color4) const {
        if (!shader_program_) {
            LOGE("shader_program is null.");
            return;
        }

        glUseProgram(shader_program_);
//
//        glActiveTexture(GL_TEXTURE0);
//        glUniform1i(texture_uniform_, 0);
//        glBindTexture(GL_TEXTURE_2D, texture_id_);
//
//        glm::mat4 mvp_mat = projection_mat * view_mat * model_mat;
//        glm::mat4 mv_mat = view_mat * model_mat;
//        glm::vec4 view_light_direction = glm::normalize(mv_mat * kLightDirection);
//
//        glUniform4f(lighting_param_uniform_, view_light_direction[0],
//                    view_light_direction[1], view_light_direction[2], 1.f);
//        glUniform4f(material_param_uniform_, ambient_, diffuse_, specular_,
//                    specular_power_);
//        glUniform4fv(color_correction_param_uniform_, 1, color_correction4);
//        glUniform4fv(color_uniform_, 1, object_color4);
//
//        glUniformMatrix4fv(mvp_mat_uniform_, 1, GL_FALSE, glm::value_ptr(mvp_mat));
//        glUniformMatrix4fv(mv_mat_uniform_, 1, GL_FALSE, glm::value_ptr(mv_mat));
//
//        // Occlusion parameters.
//        if (use_depth_for_occlusion_) {
//            // Attach the depth texture.
//            glActiveTexture(GL_TEXTURE1);
//            glBindTexture(GL_TEXTURE_2D, depth_texture_id_);
//            glUniform1i(depth_texture_uniform_, 1);
//
//            // Set the depth texture uv transform.
//            glUniformMatrix3fv(depth_uv_transform_uniform_, 1, GL_FALSE,
//                               glm::value_ptr(uv_transform_));
//            glUniform1f(depth_aspect_ratio_uniform_, depth_aspect_ratio_);
//        }
//
//        // Note: for simplicity, we are uploading the model each time we draw it.  A
//        // real application should use vertex buffers to upload the geometry once.


        glEnableVertexAttribArray(position_attrib_);
        glVertexAttribPointer(position_attrib_, 3, GL_FLOAT, GL_FALSE, 0,
                              vertices_.data());
//
<<<<<<< Updated upstream
        glEnableVertexAttribArray(normal_attrib_);
        glVertexAttribPointer(normal_attrib_, 3, GL_FLOAT, GL_FALSE, 0,
                              normals_.data());
=======
//        glEnableVertexAttribArray(normal_attrib_);
//        glVertexAttribPointer(normal_attrib_, 3, GL_FLOAT, GL_FALSE, 0,
//                              normals_.data());
>>>>>>> Stashed changes
//
//        glEnableVertexAttribArray(tex_coord_attrib_);
//        glVertexAttribPointer(tex_coord_attrib_, 2, GL_FLOAT, GL_FALSE, 0,
//                              uvs_.data());

<<<<<<< Updated upstream
        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);

        // Textures are loaded with premultiplied alpha
        // (https://developer.android.com/reference/android/graphics/BitmapFactory.Options#inPremultiplied),
        // so we use the premultiplied alpha blend factors.
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
=======
//        glDepthMask(GL_TRUE);
//        glEnable(GL_BLEND);

//        // Textures are loaded with premultiplied alpha
//        // (https://developer.android.com/reference/android/graphics/BitmapFactory.Options#inPremultiplied),
//        // so we use the premultiplied alpha blend factors.
//        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
>>>>>>> Stashed changes

        glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_SHORT,
                       indices_.data());

//        glDisable(GL_BLEND);
        glDisableVertexAttribArray(position_attrib_);
//        glDisableVertexAttribArray(tex_coord_attrib_);
        glDisableVertexAttribArray(normal_attrib_);

//        glUseProgram(0);
        CheckGlError("obj_renderer::Draw()");
    }
