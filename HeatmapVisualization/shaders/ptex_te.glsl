#version 450
#extension GL_ARB_bindless_texture : require
layout (quads) in;


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


uniform mat4 model_view_matrix;
uniform mat4 projection_matrix;

uniform vec3 cell_size;


out vec4 position;
out vec2 uv;
out vec2 neighbour_uvs[4];
flat out ivec2 neighbour_indices[4];
out vec3 patch_normal;

// uv transform matrices for all 16 cases, see "Borderless Per Face Texture Mapping"
mat3 uv_transform[16] = { mat3(-1,0,0,0,-1,0,1,0,0),
  mat3(0,-1,0,1,0,0,1,1,0),
  mat3(1,0,0,0,1,0,0,1,0),
  mat3(0,1,0,-1,0,0,0,0,0),
  mat3(0,1,0,-1,0,0,1,-1,0),
  mat3(-1,0,0,0,-1,0,2,-1,0),
  mat3(0,-1,0,1,0,0,0,2,0),
  mat3(1,0,0,0,1,0,-1,0,0),
  mat3(1,0,0,0,1,0,0,-1,0),
  mat3(0,1,0,-1,0,0,2,0,0),
  mat3(-1,0,0,0,-1,0,1,2,0),
  mat3(0,-1,0,1,0,0,-1,1,0),
  mat3(0,-1,0,1,0,0,0,0,0),
  mat3(1,0,0,0,1,0,1,0,0),
  mat3(0,1,0,-1,0,0,1,1,0),
  mat3(-1,0,0,0,-1,0,0,1,0)
};

void main(void)
{
    vec4 p_0 = mix(gl_in[0].gl_Position,
                  gl_in[1].gl_Position,
                  gl_TessCoord.x);
    vec4 p_1 = mix(gl_in[3].gl_Position,
                  gl_in[2].gl_Position,
                  gl_TessCoord.x);
    position = mix(p_0, p_1, gl_TessCoord.y);

    uv = vec2(gl_TessCoord.x,gl_TessCoord.y);

    patch_normal = normalize(cross(vec3(gl_in[1].gl_Position - gl_in[0].gl_Position),vec3(gl_in[3].gl_Position-gl_in[0].gl_Position)));

    PtexParameters params = ptex_params[gl_PrimitiveID];

    //    //DEBUGGING
    //    //params.ngbr_ptex_param_indices[0] = -1;
    //    //params.ngbr_ptex_param_indices[1] = -1;
    //    //params.ngbr_ptex_param_indices[2] = -1;
    //    //params.ngbr_ptex_param_indices[3] = -1;
    //    //params.texture_index = 0;
    //    //params.base_slice = 0;
      
    // get values from neighbours
    for(int i=0; i<4; ++i)
    {
      if(params.ngbr_ptex_param_indices[i] != -1)
      {
        // get neighbour params
        PtexParameters neighbour_params = ptex_params[params.ngbr_ptex_param_indices[i]];

        // for each neighbour get uv coordinates and texture indices and pass on to fragment shader
        neighbour_uvs[i] = (uv_transform[neighbour_params.ngbr_uv_transform_cases[i]] * vec3(uv,1.0)).xy;
        neighbour_indices[i] = ivec2(neighbour_params.texture_index,neighbour_params.base_slice);
      }
    }

    position = model_view_matrix * position;
    gl_Position = projection_matrix * position;
}
