#version 330

// fog_render.fs — Circular reveal mask untuk minimap fog.
// - texture0: fogRT (full-res render texture, fog tiles pre-drawn)
// - circleCenter/circleRadius: reveal circle di fogRT pixel coords

in vec2 fragTexCoord;
out vec4 finalColor;

uniform sampler2D texture0;
uniform vec2 circleCenter;
uniform float circleRadius;

void main()
{
    vec4 state = texture(texture0, fragTexCoord);
    float fogAlpha = state.a;

    // Circular reveal mask — smooth edge (2px transition band)
    vec2 texSize = vec2(textureSize(texture0, 0));
    vec2 pixelPos = fragTexCoord * texSize;
    float dist = distance(pixelPos, circleCenter);
    float mask = smoothstep(circleRadius - 1.0, circleRadius + 1.0, dist);

    finalColor = vec4(state.rgb, fogAlpha * mask);
}
