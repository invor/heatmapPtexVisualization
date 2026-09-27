#version 450
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader5 : require 
#extension GL_ARB_gpu_shader_int64 : enable
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


// texture handle buffer
//layout(std430, binding = 2) buffer ptexTexturesBuffer { uint64_t[] ptex_textures; };
layout(std430, binding = 2) buffer ptexTexturesBuffer { uvec2[] ptex_textures; };

// ptex parameter buffer
layout(std430, binding = 3) buffer ptexParametersBuffer { PtexParameters[] ptex_params; };

in vec4 position;
in vec2 uv;
in vec2 neighbour_uvs[4];
flat in ivec2 neighbour_indices[4];
in vec3 patch_normal;

layout (location = 0) out vec4 out_colour;


float sdBox( in vec2 p, in vec2 b )
{
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

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

void main()
{
  PtexParameters params = ptex_params[gl_PrimitiveID];
  
  ivec3 tex_size = textureSize(sampler2DArray(ptex_textures[params.texture_index]),0);

  vec4 colour = texelFetch(sampler2DArray(ptex_textures[params.texture_index]),ivec3(ivec2(uv*tex_size.xy),int(params.base_slice)),0);
  //vec4 colour = texture(sampler2DArray(ptex_textures[params.texture_index]),vec3(uv,float(params.base_slice))); 

  //vec4 colour = vec4(
  //  float(params.ngbr_ptex_param_indices[0])/384.0,
  //  float(params.ngbr_ptex_param_indices[1])/384.0,
  //  float(params.ngbr_ptex_param_indices[2])/384.0,
  //  1.0
  //  );

  float border_sdf = sdBox(uv - vec2(0.5), vec2( 0.5 - 0.5*fwidth(uv) ));
  float axis_x_sdf = sdBox(uv - vec2(0.3,0.1), vec2(0.155,0.035));
  float axis_y_sdf = sdBox(uv - vec2(0.1,0.3), vec2(0.035,0.155));

  vec3 base_colour = vec3(0.28);
  //if(border_sdf > 0.0 || axis_x_sdf < 0.0 || axis_y_sdf < 0.0)
  if(border_sdf > 0.0)
  {
    base_colour = vec3(0.0);
  }

  //if(axis_x_sdf < -0.005)
  //{
  //  colour = vec4(1.0,0.1,0.1,1.0);
  //}

  //if(axis_y_sdf < -0.005)
  //{
  //  colour = vec4(0.1,1.0,0.1,1.0);
  //}

  float pseudo_lightning = 
    clamp(dot(normalize(patch_normal),normalize(vec3(-1.0,1.0,-1.0))),0.0,1.0) * 0.3 + //light source 1
    clamp(dot(normalize(patch_normal),normalize(vec3(1.0,1.0,-1.0))),0.0,1.0) * 0.3 + //light source 2
    0.4; // ambient

  //pseudo_lightning = 1.0;
  out_colour.rgb = colour.a * colour.rgb + (1.0 - colour.a) * vec3(base_colour * pseudo_lightning);
  //out_colour = vec4(colour.rgb * pseudo_lightning, 1.0);

  //  Temporary tone mapping
  //out_colour.rgb = out_colour.rgb/(1.0+out_colour.rgb);
  return;

  // get values from neighbours
  for(int i=0; i<4; ++i)
  {
    if( params.ngbr_ptex_param_indices[i] != -1)
    {
      uint tex_idx = neighbour_indices[i].x;
      float base_slice = float(neighbour_indices[i].y);
      vec2 uv = neighbour_uvs[i];
      uvec2 tex_handle = ptex_textures[tex_idx];

      vec4 neighbour_colour = texture(sampler2DArray(tex_handle),vec3(uv,base_slice));
      
      colour += neighbour_colour;
    }
  }

  out_colour = colour / colour.a;
}