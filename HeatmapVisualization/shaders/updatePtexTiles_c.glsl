#version 450
#extension GL_ARB_bindless_texture : require

struct PtexParameters
{
  int ngbr_ptex_param_indices[4]; // index into PtexParameters buffers
  int ngbr_uv_transform_cases[4]; // uv transform cases for the 4 direc neighbours

  // Save space by storing index into buffer with all texture handles instead of storing 64bit handles
  // (for texture arrays handle is shared by many faces)
  uint texture_index; // index into texture indirection buffer (where all bindless texture handles are stored)
  uint base_slice; // slice in texture array
};

struct TextureTile{
  uint tex_index;
  uint base_slice;
};

// vertex + index data of mesh with embedded material ID and noise parameters
layout(std430, binding = 0) readonly buffer VertexBuffer { float[] vertices; };
layout(std430, binding = 1) readonly buffer IndexBuffer { uint[] indices; };

// ptex image handle buffer
//layout(std430, binding = 2) buffer ptexImagesBuffer { layout(rgba8) image2DArray[] ptex_images; };
layout(std430, binding = 2) readonly buffer ptexImagesBuffer { uvec2[] ptex_images; };
// ptex parameter buffer
layout(std430, binding = 3) writeonly buffer ptexParametersBuffer { PtexParameters[] ptex_params; };

// gaze data buffer
layout(std430, binding = 4) readonly buffer gazeDataBuffer { float[] gaze_data; };

// update patches
layout(std430, binding = 6) readonly buffer updatePatchesBuffer { uint[] update_patches; };
// free slots
layout(std430, binding = 7) readonly buffer textureTilesBuffer { TextureTile[] texture_tiles; };

uniform float texture_lod;
uniform int update_patch_offset;
uniform int texture_slot_offset;

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

vec3 inferno(float t)
{
  vec3 inferno_lut[8] = {
    vec3(0.0014619955811715805,0.0004659913919114934,0.013866005775115809),
    vec3(0.15878054505364145,0.04414588479176828,0.32873705502988054),
    vec3(0.396786518835543,0.08292103408227261,0.4331726873798219),
    vec3(0.6234475076301247,0.16486328557646127,0.3880663468322876),
    vec3(0.8308925639196657,0.28265548598550927,0.2586364361687295),
    vec3(0.9615932007416385,0.4896799300459282,0.08356448400711391),
    vec3(0.9816315597243276,0.7558372599499625,0.15291300331162103),
    vec3(0.9883620799212208,0.9983616470620554,0.6449240982803861)
  };

  float v = clamp(t, 0.0, 1.0) * 7.0;
  int i = int(floor(v));
  float lambda = v-i;

  return inferno_lut[i] * (1.0-lambda) + inferno_lut[ min(i+1,7) ] * (lambda);
}

vec3 blackBody(float t)
{
  vec3 black_body_lut[8] = {
    vec3(0.0,0.0,0.0),
    vec3(0.2567618382302789,0.08862237092250158,0.06900234709883349),
    vec3(0.502299529628274,0.12275205976842546,0.10654041357261984),
    vec3(0.7353154662963063,0.1982320329476474,0.12428036101896534),
    vec3(0.8771435867383445,0.39490510462624345,0.03816328606394868),
    vec3(0.911232394909533,0.631724377007152,0.10048201891972874),
    vec3(0.9072006655243174,0.8550025783221541,0.18879408728283467),
    vec3(1.0,1.0,1.0)
  };

  float v = clamp(t, 0.0, 1.0) * 7.0;
  int i = int(floor(v));
  float lambda = v-i;

  return black_body_lut[i] * (1.0-lambda) + black_body_lut[ min(i+1,7) ] * (lambda);
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main()
{
  ivec3 gID = ivec3(gl_GlobalInvocationID);
  
  vec2 tile_res = vec2(float(gl_NumWorkGroups.x) * 8.0, float(gl_NumWorkGroups.y) * 8.0);
  
  uint tgt_primtive_idx = update_patches[update_patch_offset + gID.z];
  
  uint quad_indices[4];
  vec3 quad_vertices[4];
   
  for(int i=0; i<4; ++i)
  {
      quad_indices[i] = indices[tgt_primtive_idx*4+i];
      quad_vertices[i].x = vertices[ quad_indices[i] * 3 + 0];
      quad_vertices[i].y = vertices[ quad_indices[i] * 3 + 1];
      quad_vertices[i].z = vertices[ quad_indices[i] * 3 + 2];
  }
  // compute texel position in world space from gID.xy and vertices
  vec2 uv = vec2(gID.xy) / (tile_res - vec2(1.0)) + (vec2(0.5)/tile_res); 
  vec3 p_0 = mix(quad_vertices[0],
                  quad_vertices[1],
                  uv.x);
  vec3 p_1 = mix(quad_vertices[3],
                  quad_vertices[2],
                  uv.x);
  vec3 texel_position = mix(p_0, p_1, uv.y);

  // get ptex index and slice from src primitive (i.e. the one that freed a texture tile slot)
  uint ptex_index = texture_tiles[texture_slot_offset + gID.z].tex_index;
  uint ptex_slice = texture_tiles[texture_slot_offset + gID.z].base_slice;
  

  float intensity = 0.0;
  // compute heatmap value from gaze data
  //for(int i=0; i<gaze_data_row_cnt;++i)
  for(int i=0; i < min(3072,gaze_data_row_cnt);++i)
  {
    //gaze_data[i*gaze_data_column_cnt + 0] // index
    float timestamp = gaze_data[i*gaze_data_column_cnt + 1]; // timestamp
    float gp_x = -gaze_data[i*gaze_data_column_cnt + 2] - 0.1;//manuel correction for mesh alignment
    float gp_y = gaze_data[i*gaze_data_column_cnt + 3];
    float gp_z = gaze_data[i*gaze_data_column_cnt + 4];

    float texel_to_gaze_point = distance(texel_position,vec3(gp_x,gp_y,gp_z));

    intensity += smoothstep(0.1, 0.0, texel_to_gaze_point);
  }
  intensity /= 25.0;

  // All texture (per tile) are kept within the same Texture2DArray
  //layout(rgba8) writeonly image2DArray ptex_image = layout(rgba8) writeonly image2DArray(ptex_images[ptex_index]); // NVIDIA
  image2DArray ptex_image = image2DArray(ptex_images[ptex_index]); // AMD
  //imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(float(ptex_index)/20.0,float(ptex_slice)/2048.0,0.0,1.0));
  //imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(texel_position,1.0));
  //imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(float(gl_LocalInvocationID.x),0.0,0.0,1.0));
  //imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(float(ptex_index)/30.0,float(ptex_slice)/2048.0,0.0,1.0));
  imageStore(ptex_image,ivec3(gID.x,gID.y,ptex_slice),vec4(viridis(intensity),1.0));
  
  ptex_params[tgt_primtive_idx].texture_index = ptex_index;
  ptex_params[tgt_primtive_idx].base_slice = ptex_slice; 
  
}