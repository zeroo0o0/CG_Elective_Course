#version 430

in vec2 UV;
in vec3 Normal;
in vec3 FragPos;

out vec4 color;

uniform sampler2D myTextureSampler;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform bool useTexture;
uniform vec3 baseColor;

// Directional Light
uniform vec3 dirLightDirection;
uniform vec3 dirLightColor;

// Spotlight
uniform vec3 spotlight1Pos;
uniform vec3 spotlight1Dir;
uniform vec3 spotlight1Color;
uniform vec3 spotlight2Pos;
uniform vec3 spotlight2Dir;
uniform vec3 spotlight2Color;
uniform float spotlightCutoff;
uniform float spotlightOuterCutoff;

uniform bool twoSided;

uniform float screenBrightness;

void main()
{
    // Ambient (controlled by ambientColor uniform)
    vec3 ambient = ambientColor;

    // Point Light (original)
    vec3 norm = normalize(Normal);
    if (twoSided && !gl_FrontFacing) {
        norm = -norm;
    }
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Point Light)
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Directional Light calculation
    vec3 dLightDir = normalize(-dirLightDirection);
    float dDiff = max(dot(norm, dLightDir), 0.0);
    vec3 dDiffuse = dDiff * dirLightColor;
    
    vec3 dReflectDir = reflect(-dLightDir, norm);
    float dSpec = pow(max(dot(viewDir, dReflectDir), 0.0), 32);
    vec3 dSpecular = specularStrength * dSpec * dirLightColor;

    // Spotlight calculation
    vec3 sDiffuse = vec3(0.0);
    vec3 sSpecular = vec3(0.0);
    float epsilon = spotlightCutoff - spotlightOuterCutoff;

    vec3 spot1Dir = normalize(FragPos - spotlight1Pos);
    float theta1 = dot(spot1Dir, normalize(spotlight1Dir));
    if(theta1 > spotlightOuterCutoff) {
        float intensity1 = clamp((theta1 - spotlightOuterCutoff) / epsilon, 0.0, 1.0);
        vec3 sLightDir1 = normalize(spotlight1Pos - FragPos);
        float sDiff1 = max(dot(norm, sLightDir1), 0.0);
        vec3 sDiffuse1 = sDiff1 * spotlight1Color * intensity1;
        vec3 sReflectDir1 = reflect(-sLightDir1, norm);
        float sSpec1 = pow(max(dot(viewDir, sReflectDir1), 0.0), 32);
        vec3 sSpecular1 = specularStrength * sSpec1 * spotlight1Color * intensity1;
        float distance1 = length(spotlight1Pos - FragPos);
        float attenuation1 = 1.0 / (1.0 + 0.09 * distance1 + 0.032 * distance1 * distance1);
        sDiffuse += sDiffuse1 * attenuation1;
        sSpecular += sSpecular1 * attenuation1;
    }

    vec3 spot2Dir = normalize(FragPos - spotlight2Pos);
    float theta2 = dot(spot2Dir, normalize(spotlight2Dir));
    if(theta2 > spotlightOuterCutoff) {
        float intensity2 = clamp((theta2 - spotlightOuterCutoff) / epsilon, 0.0, 1.0);
        vec3 sLightDir2 = normalize(spotlight2Pos - FragPos);
        float sDiff2 = max(dot(norm, sLightDir2), 0.0);
        vec3 sDiffuse2 = sDiff2 * spotlight2Color * intensity2;
        vec3 sReflectDir2 = reflect(-sLightDir2, norm);
        float sSpec2 = pow(max(dot(viewDir, sReflectDir2), 0.0), 32);
        vec3 sSpecular2 = specularStrength * sSpec2 * spotlight2Color * intensity2;
        float distance2 = length(spotlight2Pos - FragPos);
        float attenuation2 = 1.0 / (1.0 + 0.09 * distance2 + 0.032 * distance2 * distance2);
        sDiffuse += sDiffuse2 * attenuation2;
        sSpecular += sSpecular2 * attenuation2;
    }

    vec3 texColor = useTexture ? texture(myTextureSampler, UV).rgb : baseColor;
    vec3 result = (ambient + diffuse + specular + dDiffuse + dSpecular + sDiffuse + sSpecular) * texColor;
    
    // 亮度数字叠加显示已关闭

    color = vec4(result, 1.0);
}

