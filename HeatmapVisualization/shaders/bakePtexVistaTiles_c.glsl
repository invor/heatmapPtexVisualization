#version 450
#extension GL_ARB_bindless_texture : require
//#extension GL_ARB_gpu_shader5 : require 
//#extension GL_ARB_gpu_shader_int64 : enable
//#extension AMD_gpu_shader_int64 : enable

struct PtexParameters
{
  int ngbr_ptex_param_indices[4]; // index into PtexParameters buffers
  int ngbr_uv_transform_cases[4]; // uv transform cases for the 4 direc neighbours

  // Save space by storing index into buffer with all texture handles instead of storing 64bit handles
  // (for texture arrays handle is shared by many faces)
  uint texture_index; // index into texture indirection buffer (where all bindless texture handles are stored)
  uint base_slice; // slice in texture array
};

// vertex + index data of mesh with embedded material ID and noise parameters
layout(std430, binding = 0) buffer VertexBuffer { float[] vertices; };
layout(std430, binding = 1) buffer IndexBuffer { uint[] indices; };

// ptex image handle buffer
//layout(std430, binding = 2) buffer ptexImagesBuffer { layout(rgba8) image2DArray[] ptex_images; };
layout(std430, binding = 2) buffer ptexImagesBuffer { uvec2[] ptex_images; };

// gaze data buffer
layout(std430, binding = 4) readonly buffer gazeDataBuffer { float[] gaze_data; };

uniform int primitive_base_idx;
uniform int texture_base_idx;
uniform int layers;

uniform int gaze_data_column_cnt;
uniform int gaze_data_row_cnt;

vec3 viridis(float t) {
  vec3 viridis_lut[8] = {
    vec3(0.2670039853213788,0.0048725657145795975,0.32941506855247793),
    vec3(0.2747410319947279,0.19697326735916815,0.49725044340782604),
    vec3(0.21267123715447978,0.3591013770537536,0.5516350468677014),
    vec3(0.15295809873202398,0.4980514512730651,0.5576853269081522),
    vec3(0.1220535918163036,0.6321055429812599,0.5308488657247317),
    vec3(0.2900139372832507,0.7588451185052187,0.4278271609212135),
    vec3(0.6221823410626537,0.8538142928663974,0.22624791114964743),
    vec3(0.9932481489335602,0.9061547634208059,0.14393594366968385)
  };

  float v = clamp(t, 0.0, 1.0) * 7.0;
  int i = int(floor(v));
  float lambda = v-i;

  return viridis_lut[i] * (1.0-lambda) + viridis_lut[ min(i+1,7) ] * (lambda);
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
    ivec3 gID = ivec3(gl_GlobalInvocationID);

    vec2 tile_res = vec2(float(gl_NumWorkGroups.x) * 8.0, float(gl_NumWorkGroups.y) * 8.0);

    int primtive_idx = primitive_base_idx + gID.z;

    uint quad_indices[4];
    vec3 quad_vertices[4];

    for(int i=0; i<4; ++i)
    {
        quad_indices[i] = indices[primtive_idx*4+i];

        quad_vertices[i].x = vertices[ quad_indices[i] * 3 + 0];
        quad_vertices[i].y = vertices[ quad_indices[i] * 3 + 1];
        quad_vertices[i].z = vertices[ quad_indices[i] * 3 + 2];
    }

    vec2 uv = vec2(float(gID.x), float(gID.y)) / (tile_res - vec2(1.0)) + (vec2(0.5)/tile_res); ;
    vec3 p_0 = mix(quad_vertices[0],
                  quad_vertices[1],
                  uv.x);
    vec3 p_1 = mix(quad_vertices[3],
                  quad_vertices[2],
                  uv.x);
    vec3 texel_position = mix(p_0, p_1, uv.y);

    float intensity = 0.0;
  // compute heatmap value from gaze data
  //for(int i=0; i<gaze_data_row_cnt;++i)
  for(int i=0; i < min(3072,gaze_data_row_cnt);++i)
  {
    //gaze_data[i*gaze_data_column_cnt + 0] // index
    float timestamp = gaze_data[i*gaze_data_column_cnt + 1]; // timestamp
    float gp_x = -gaze_data[i*gaze_data_column_cnt + 2] - 0.1;//manual correction for mesh alignment
    float gp_y = gaze_data[i*gaze_data_column_cnt + 3];
    float gp_z = gaze_data[i*gaze_data_column_cnt + 4];

    float texel_to_gaze_point = distance(texel_position,vec3(gp_x,gp_y,gp_z));

    intensity += smoothstep(0.1, 0.0, texel_to_gaze_point);
  }
  intensity /= 25.0;
  
    int ptex_index = (primtive_idx) / layers;
    int ptex_slice = (primtive_idx) - (ptex_index * layers);
    ptex_index += texture_base_idx;

    // All texture (per tile) are kept within the same Texture2DArray
    //layout(rgba8) writeonly image2DArray ptex_image = layout(rgba8) writeonly image2DArray(ptex_images[ptex_index]); // NVIDIA
    image2DArray ptex_image = image2DArray(ptex_images[ptex_index]); // AMD

    imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(viridis(intensity),1.0));
}