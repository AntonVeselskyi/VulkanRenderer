#version 450 //use GLSL 4.5
//point in space is passed as input

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(binding = 0) uniform UBOViewProjection
{
	mat4 projection;
	mat4 view;
} ubo_vp;

layout(binding = 1) uniform UBOModel
{
	mat4 model;
} ubo_model;


layout(location = 0) out vec3 fragment_color;

void main()
{
	gl_Position = ubo_vp.projection * ubo_vp.view * ubo_model.model * vec4(position, 1.0);
	fragment_color = color;
}