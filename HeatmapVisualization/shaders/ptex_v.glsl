#version 430

in vec3 v_position;

void main()
{
	gl_Position = vec4(v_position.xyz, 1.0);
}