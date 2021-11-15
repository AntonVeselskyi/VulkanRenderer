#version 450 //use GLSL 4.5
//point in space is passed as input

layout(location = 0) out vec3 fragColour; //out put color for vertex

// Triangle vertex position
//(will put into vertex buffer later)
vec3 positions[3] = vec3[]
(
	//screen coordinates between 1 and -1
	vec3(0.0, -0.4, 0.0),
	vec3(0.4, 0.4, 0.0),
	vec3(-0.4, 0.4, 0.0)
);

//Triangle vertex colors
vec3 colours[3] = vec3[]
(
	//RGB values are normalized
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0)
);


void main()
{
	//output point position variable
	gl_Position = vec4(positions[gl_VertexIndex], 1.0);
	fragColour = colours[gl_VertexIndex];
}