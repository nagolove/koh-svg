// vim: set colorcolumn=85
// vim: fdm=marker
#version 430

uniform float iTime;
uniform vec3 iResolution;

in vec2 fragTexCoord;

// Output fragment color
out vec4 finalColor;

void main() {
    //vec3 rd = normalize(vec3(2.*fragTexCoord - iResolution.xy, iResolution.y));
    finalColor = vec4(clamp(sin(iTime), 0., 1.), 0.1, 0.1, 1.);
    //finalColor = vec4(.0, 1, 0.1, 1.);
}

