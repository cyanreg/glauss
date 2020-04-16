#version 100

precision highp float;

uniform int draw_mode;
uniform sampler2D spriteTexture;
uniform vec4 objcolor;

void main() {
    if (draw_mode == 1) {
        vec4 texture = texture2D(spriteTexture, gl_PointCoord)*objcolor;
        gl_FragColor = texture;
    } else {
        gl_FragColor = objcolor;
    }
    //gl_FragColor=texture2D(texture, gl_PointCoord);
}
