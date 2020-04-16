#version 100

precision highp float;

varying vec2 texcoord;
uniform sampler2D tex;
uniform vec4 textcolor;

void main(void) {
    gl_FragColor = vec4(1, 1, 1, texture2D(tex, texcoord).a)*textcolor;
}
