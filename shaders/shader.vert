#version 450 //use GLSL 4.5
//point in space is passed as input

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragment_color;

void main()
{
	gl_Position = vec4(position, 1.0);
	fragment_color = color;
}