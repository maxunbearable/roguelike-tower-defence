#version 330

// Atmosphere pass for the play field.
//
// The board was lit perfectly flatly and sat in a saturated lime green, which
// read as a cheerful lawn rather than anywhere dangerous. This does three
// things, in this order:
//
//   1. Grades the colour toward a cool/warm duotone ramp, pulling the green down
//      and giving shadows a cold cast.
//   2. Multiplies in an ambient tint, darkening the whole field.
//   3. Adds light back only around the towers, so the towers are the reason the
//      board is visible at all.
//
// A vignette finishes it, so the eye lands on the middle of the field.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

out vec4 finalColor;

#define MAX_LIGHTS 12

// The HUD band occupies the bottom of the frame and must come through
// untouched: grading and vignetting a UI panel looks like a bug, not a mood.
uniform float uHudFrac;
uniform vec2 uVirtual;      // virtual resolution, for world-space light maths
uniform float uDesat;       // how far toward greyscale
uniform float uCool;        // how cold the SHADOWS go; midtones keep their hue
uniform float uVignette;
uniform vec3 uAmbient;      // ambient multiplier; below 1 darkens the field
uniform float uLightGain;   // how strongly a tower's pool brightens the ground

uniform int uLightCount;
uniform vec2 uLightPos[MAX_LIGHTS];
uniform vec3 uLightColor[MAX_LIGHTS];
uniform float uLightRadius[MAX_LIGHTS];

void main() {
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    // The render texture is blitted with a negative source height, so v runs 1
    // at the top of the screen down to 0 at the bottom. The HUD is therefore
    // the LOW end of v, not the high end.
    if (fragTexCoord.y < uHudFrac) {
        finalColor = texel;
        return;
    }

    vec3 rgb = texel.rgb;

    // --- 1. grade ---------------------------------------------------------
    // A full duotone ramp was the first attempt and it flattened the board to a
    // uniform grey-olive: it replaces hue instead of shaping it, and it pushed
    // the warm dirt path to pink. Instead: desaturate modestly, then cool only
    // the SHADOWS, multiplicatively, so grass stays green and dirt stays earth.
    float luma = dot(rgb, vec3(0.299, 0.587, 0.114));
    rgb = mix(rgb, vec3(luma), uDesat);

    const vec3 coldTint = vec3(0.70, 0.85, 1.08);
    float shadowW = 1.0 - smoothstep(0.0, 0.58, luma);
    rgb = mix(rgb, rgb * coldTint, shadowW * uCool);

    // --- 2. ambient darkening --------------------------------------------
    rgb *= uAmbient;

    // --- 3. light pools ---------------------------------------------------
    // Fragment position in virtual pixels, which is the space the tower
    // positions arrive in.
    vec2 pv = vec2(fragTexCoord.x * uVirtual.x, (1.0 - fragTexCoord.y) * uVirtual.y);
    vec3 gathered = vec3(0.0);
    for (int i = 0; i < uLightCount && i < MAX_LIGHTS; ++i) {
        float d = distance(pv, uLightPos[i]);
        float fall = 1.0 - clamp(d / max(uLightRadius[i], 1.0), 0.0, 1.0);
        fall *= fall;  // quadratic, so the pool has a soft edge
        gathered += uLightColor[i] * fall;
    }
    // Overlapping pools must SATURATE, not sum. Seven adjacent towers summed to
    // roughly 7x here and blew the entire cluster out to flat white -- the
    // towers, the ground and the props with it. x/(1+x) rolls off smoothly
    // toward 1 no matter how many lights overlap, so a dense cluster is bright
    // but never clipped.
    gathered = gathered / (1.0 + gathered);

    // Multiply-add rather than plain add: light reveals what is already there
    // instead of washing it out to white.
    rgb += rgb * gathered * uLightGain;

    // --- vignette ---------------------------------------------------------
    float py = (fragTexCoord.y - uHudFrac) / max(1.0 - uHudFrac, 0.001);
    vec2 q = vec2(fragTexCoord.x, py) - vec2(0.5);
    float vig = 1.0 - uVignette * dot(q, q) * 2.0;
    rgb *= clamp(vig, 0.0, 1.0);

    finalColor = vec4(rgb, texel.a);
}
