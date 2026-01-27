#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

#define MAX_LIGHTS 32

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

uniform Material material;
uniform PointLight pointLights[MAX_LIGHTS];
uniform int numLights;
uniform vec3 viewPos;
uniform bool thermalVision; 

void main()
{
    vec3 norm = normalize(Normal);
    vec3 diffTex = texture(material.diffuse, TexCoords).rgb;
    
    // 1. Iluminación normal
    vec3 lighting = 0.05 * diffTex; 
    for (int i = 0; i < numLights; i++)
    {
        vec3 lightDir = normalize(pointLights[i].position - FragPos);
        float dist = length(pointLights[i].position - FragPos);
        float atten = 1.0 / (1.0 + 0.7 * dist + 1.8 * (dist * dist));
        float diff = max(dot(norm, lightDir), 0.0);
        lighting += diff * pointLights[i].color * diffTex * pointLights[i].intensity * atten;
    }

    if(thermalVision)
    {
        // --- VISIÓN NOCTURNA MILITAR OSCURA ---
        // Extraemos la luminancia
        float grayscale = dot(diffTex, vec3(0.2126, 0.7152, 0.0722));
        
        // Multiplicador de brillo (ajusta este 1.4 si lo quieres más oscuro aún)
        float brightness = grayscale * 1.4;

        // Color verde militar oscuro (Fósforo P43)
        // Menos azul, más verde oliva/esmeralda profundo
        vec3 nightVisionColor = vec3(0.05, 0.45, 0.1); 
        
        // Aplicamos un contraste extra para que las sombras no se pierdan
        vec3 finalColor = nightVisionColor * brightness;
        finalColor = pow(finalColor, vec3(1.1)); // Oscurece los tonos medios

        FragColor = vec4(finalColor, 1.0);
    }
    else
    {
        // --- MODO NORMAL ---
        float distCam = length(viewPos - FragPos);
        float fogFactor = exp(-distCam * 0.04);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        
        vec3 fogColor = vec3(0.01, 0.01, 0.02); 
        vec3 finalColor = mix(fogColor, lighting, fogFactor);
        
        FragColor = vec4(pow(finalColor, vec3(1.0/1.2)), 1.0);
    }
}