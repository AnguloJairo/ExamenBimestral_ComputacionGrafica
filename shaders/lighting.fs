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

// --- NUEVO: Interruptor para objetos que brillan (como la luna) ---
uniform bool isEmissive;
// -----------------------------------------------------------------

void main()
{
    // 1. LEEMOS LA TEXTURA COMPLETA (RGBA)
    vec4 texColor = texture(material.diffuse, TexCoords);

    // 2. EL TRUCO MAGICO: "DISCARD"
    // Si la transparencia (alpha) es menor al 10%, descartamos el píxel.
    // Esto hace que se vean los agujeros.
    if(texColor.a < 0.1)
        discard;

    // 3. Continuamos con la lógica normal usando el RGB de la textura
    vec3 diffTex = texColor.rgb; 
    vec3 norm = normalize(Normal);

    //vec3 norm = normalize(Normal);
    //vec3 diffTex = texture(material.diffuse, TexCoords).rgb;
    
    // 1. Iluminación normal (calculamos esto siempre, pero lo usaremos según el caso)
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
        // (Esto funcionará bien con la luna porque usa 'diffTex' directamente.
        //  La luna se verá verde brillante, lo cual es realista para visión nocturna).

        // Extraemos la luminancia
        float grayscale = dot(diffTex, vec3(0.2126, 0.7152, 0.0722));
        
        // Multiplicador de brillo (ajusta este 1.4 si lo quieres más oscuro aún)
        float brightness = grayscale * 1.4;

        // Color verde militar oscuro (Fósforo P43)
        vec3 nightVisionColor = vec3(0.05, 0.45, 0.1); 
        
        // Aplicamos un contraste extra para que las sombras no se pierdan
        vec3 finalColor = nightVisionColor * brightness;
        finalColor = pow(finalColor, vec3(1.1)); // Oscurece los tonos medios

        FragColor = vec4(finalColor, 1.0);
    }
    else
    {
        // --- MODO NORMAL ---

        // --- NUEVA LÓGICA AQUÍ ---
        if (isEmissive) 
        {
            // Si es la luna (emisivo), ignoramos la niebla y las luces.
            // Devolvemos el color puro de la textura para que brille.
            FragColor = vec4(diffTex, 1.0); 
        }
        else 
        {
            // Si es la casa o el suelo, aplicamos niebla y luz normal
            float distCam = length(viewPos - FragPos);
            float fogFactor = exp(-distCam * 0.04);
            fogFactor = clamp(fogFactor, 0.0, 1.0);
            
            vec3 fogColor = vec3(0.01, 0.01, 0.02); 
            vec3 finalColor = mix(fogColor, lighting, fogFactor);
            
            FragColor = vec4(pow(finalColor, vec3(1.0/1.2)), 1.0);
        }
        // -------------------------
    }
}