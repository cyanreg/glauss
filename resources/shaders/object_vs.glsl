#version 100

precision highp float;

uniform mat4 translMat;
uniform mat4 rotationMat;
uniform mat4 scalingMat;
uniform mat4 perspectiveMat;
uniform sampler2D spriteTexture;
uniform int draw_mode;
attribute vec4 pos;
uniform vec4 objcolor;
uniform float radius;

void main() {
    gl_Position = pos*translMat*rotationMat*scalingMat*perspectiveMat;
    gl_PointSize = radius;
}
