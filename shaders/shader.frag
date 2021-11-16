#version 450 //use GLSL 4.5
//go through all pixel

layout(location = 0) in vec3 fragment_color;

layout(location = 0) out vec4 outColour;

void main()
{
	outColour = vec4(fragment_color, 1.0);
}