#version 330

// CRT monitor effect for Tetris++: curved glass, scanlines, phosphor glow,
// a faint aperture grille, vignette and a little flicker.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform float strength;   // 0 = flat screen, 1 = full effect

out vec4 finalColor;

vec2 curve(vec2 uv) {
    uv = uv * 2.0 - 1.0;
    vec2 offset = abs(uv.yx) / vec2(5.5, 4.5);
    uv = uv + uv * offset * offset * strength;
    return uv * 0.5 + 0.5;
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 size = vec2(textureSize(texture0, 0));
    vec2 uv = curve(fragTexCoord);

    // Outside the curved glass: dark bezel
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        finalColor = vec4(0.012, 0.014, 0.012, 1.0);
        return;
    }

    vec2 px = 1.0 / size;
    vec3 col = texture(texture0, uv).rgb;

    // Slight colour fringing, like an imperfect tube
    col.r = mix(col.r, texture(texture0, uv + vec2(px.x * 0.8, 0.0)).r, 0.45 * strength);
    col.b = mix(col.b, texture(texture0, uv - vec2(px.x * 0.8, 0.0)).b, 0.45 * strength);

    // Phosphor glow: a wide, cheap blur added on top
    vec3 glow = vec3(0.0);
    for (int x = -2; x <= 2; x++) {
        for (int y = -2; y <= 2; y++) {
            glow += texture(texture0, uv + vec2(x, y) * px * 2.5).rgb;
        }
    }
    glow /= 25.0;
    col += glow * 0.65 * strength;

    // Scanlines (one per virtual pixel row) and aperture grille
    float s = sin(uv.y * size.y * 3.14159);
    col *= mix(1.0, 0.70 + 0.30 * s * s, strength);
    float grille = mod(gl_FragCoord.x, 3.0);
    vec3 mask = grille < 1.0 ? vec3(1.0, 0.88, 0.88)
              : grille < 2.0 ? vec3(0.88, 1.0, 0.88)
                             : vec3(0.88, 0.88, 1.0);
    col *= mix(vec3(1.0), mask, strength);

    // Vignette, flicker and a bit of noise
    float vig = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    col *= mix(1.0, pow(vig, 0.22), strength);
    col *= 1.0 - strength * (0.025 + 0.02 * sin(time * 97.0));
    col += (rand(uv * size + time) - 0.5) * 0.035 * strength;

    finalColor = vec4(col, 1.0) * colDiffuse;
}
