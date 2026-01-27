#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp> 
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>
#include <vector>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <learnopengl/stb_image.h>

// --- CONFIGURACIÓN ---
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 800;
const int MAX_LIGHTS = 32;
const float MAX_DISTANCE = 100.0f;
const float RESPAWN_DELAY = 2.0f;
const float ACCELERATION = 35.0f;
const float FRICTION = 0.94f;
const float MAX_SPEED = 12.0f;
const float DRONE_RADIUS = 0.3f;

// --- ESTADOS ---
struct DroneState {
    bool thermalVision = false;
    bool vKeyPressed = false;
    bool lightsOn = true;
    bool lKeyPressed = false;
    bool signalLost = false;
    float signalLostTimer = 0.0f;
    glm::vec3 velocity = glm::vec3(0.0f);
    float startTime = 0.0f;
    float batteryPercent = 100.0f;
} drone;

Camera camera(glm::vec3(0.0f, 2.0f, 15.0f));
const glm::vec3 SPAWN_POINT = glm::vec3(0.0f, 2.0f, 15.0f);
float lastX = SCR_WIDTH / 2.0f, lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;

struct BoundingBox { glm::vec3 min, max; };
std::vector<BoundingBox> collisionBoxes;
std::vector<glm::vec3> lampPositions;

// --- CALLBACKS ---
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    camera.ProcessMouseMovement((float)xpos - lastX, lastY - (float)ypos);
    lastX = (float)xpos; lastY = (float)ypos;
}

void ExtractData(Model& house, Model& lights) {
    collisionBoxes.clear();
    collisionBoxes.reserve(house.meshes.size());
    for (const auto& mesh : house.meshes) {
        glm::vec3 minP(1e10f), maxP(-1e10f);
        for (const auto& v : mesh.vertices) {
            minP = glm::min(minP, v.Position);
            maxP = glm::max(maxP, v.Position);
        }
        collisionBoxes.push_back({ minP, maxP });
    }

    lampPositions.clear();
    lampPositions.reserve(lights.meshes.size());
    for (const auto& mesh : lights.meshes) {
        glm::vec3 center(0.0f);
        for (const auto& v : mesh.vertices) center += v.Position;
        lampPositions.push_back(center / (float)mesh.vertices.size());
    }
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    bool vKey = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
    if (vKey && !drone.vKeyPressed) drone.thermalVision = !drone.thermalVision;
    drone.vKeyPressed = vKey;

    bool lKey = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lKey && !drone.lKeyPressed) drone.lightsOn = !drone.lightsOn;
    drone.lKeyPressed = lKey;

    bool ghostMode = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    glm::vec3 inputDir(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputDir += camera.Front;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputDir -= camera.Front;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputDir -= camera.Right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputDir += camera.Right;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) inputDir.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) inputDir.y -= 1.0f;

    if (glm::length(inputDir) > 0.0f)
        drone.velocity += glm::normalize(inputDir) * ACCELERATION * deltaTime;

    if (glm::length(drone.velocity) > MAX_SPEED)
        drone.velocity = glm::normalize(drone.velocity) * MAX_SPEED;

    drone.velocity *= FRICTION;
    glm::vec3 nextPos = camera.Position + drone.velocity * deltaTime;

    if (!ghostMode) {
        for (const auto& box : collisionBoxes) {
            if (nextPos.x + DRONE_RADIUS > box.min.x && nextPos.x - DRONE_RADIUS < box.max.x &&
                nextPos.y + DRONE_RADIUS > box.min.y && nextPos.y - DRONE_RADIUS < box.max.y &&
                nextPos.z + DRONE_RADIUS > box.min.z && nextPos.z - DRONE_RADIUS < box.max.z) {
                drone.velocity = glm::vec3(0.0f);
                return;
            }
        }
    }
    camera.Position = nextPos;
}

// --- HUD SETUP ---
unsigned int loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    stbi_set_flip_vertically_on_load(true);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else {
        std::cout << "Failed to load texture: " << path << std::endl;
        stbi_image_free(data);
    }

    stbi_set_flip_vertically_on_load(false);
    return textureID;
}

unsigned int setupQuadVAO() {
    float vertices[] = {
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return VAO;
}

unsigned int setupWarningVAO() {
    float vertices[] = {
        -0.35f,  0.12f, 0.0f, 0.35f,  0.12f, 0.0f,
         0.35f, -0.12f, 0.0f, -0.35f, -0.12f, 0.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

unsigned int setupTextVAO() {
    std::vector<float> v;
    float s = 0.015f, x = -0.28f, y = 0.0f, sp = 0.05f;

    v.insert(v.end(), { x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y + s,0, x,y + s,0, x + s,y + s,0, x + s,y + s,0, x + s,y,0, x + s,y,0, x,y,0 });
    x += sp; v.insert(v.end(), { x,y + 2 * s,0, x,y,0 });
    x += sp; v.insert(v.end(), { x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y,0, x,y,0, x + s,y,0, x + s,y,0, x + s,y + s,0, x + s,y + s,0, x + s * 0.5f,y + s,0 });
    x += sp; v.insert(v.end(), { x,y,0, x,y + 2 * s,0, x,y + 2 * s,0, x + s,y,0, x + s,y,0, x + s,y + 2 * s,0 });
    x += sp; v.insert(v.end(), { x,y,0, x + s * 0.5f,y + 2 * s,0, x + s * 0.5f,y + 2 * s,0, x + s,y,0, x + s * 0.25f,y + s,0, x + s * 0.75f,y + s,0 });
    x += sp; v.insert(v.end(), { x,y + 2 * s,0, x,y,0, x,y,0, x + s,y,0 });
    x += sp * 1.5f;
    v.insert(v.end(), { x,y + 2 * s,0, x,y,0, x,y,0, x + s,y,0 });
    x += sp; v.insert(v.end(), { x,y,0, x,y + 2 * s,0, x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y,0, x + s,y,0, x,y,0 });
    x += sp; v.insert(v.end(), { x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y + s,0, x,y + s,0, x + s,y + s,0, x + s,y + s,0, x + s,y,0, x + s,y,0, x,y,0 });
    x += sp; v.insert(v.end(), { x,y + 2 * s,0, x + s,y + 2 * s,0, x + s * 0.5f,y + 2 * s,0, x + s * 0.5f,y,0 });

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

void createDigitVertices(std::vector<float>& v, int digit, float x, float y, float s) {
    // Dibuja dígitos del 0-9 usando segmentos de 7 líneas
    // Coordenadas ajustables: x (horizontal), y (vertical), s (tamaño)
    switch (digit) {
    case 0:
        v.insert(v.end(), { x,y,0, x,y + 2 * s,0, x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y,0, x + s,y,0, x,y,0 });
        break;
    case 1:
        v.insert(v.end(), { x + s,y + 2 * s,0, x + s,y,0 });
        break;
    case 2:
        v.insert(v.end(), { x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + s,0, x + s,y + s,0, x,y + s,0, x,y + s,0, x,y,0, x,y,0, x + s,y,0 });
        break;
    case 3:
        v.insert(v.end(), { x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y,0, x + s,y,0, x,y,0, x,y + s,0, x + s,y + s,0 });
        break;
    case 4:
        v.insert(v.end(), { x,y + 2 * s,0, x,y + s,0, x,y + s,0, x + s,y + s,0, x + s,y + 2 * s,0, x + s,y,0 });
        break;
    case 5:
        v.insert(v.end(), { x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y + s,0, x,y + s,0, x + s,y + s,0, x + s,y + s,0, x + s,y,0, x + s,y,0, x,y,0 });
        break;
    case 6:
        v.insert(v.end(), { x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y,0, x,y,0, x + s,y,0, x + s,y,0, x + s,y + s,0, x + s,y + s,0, x,y + s,0 });
        break;
    case 7:
        v.insert(v.end(), { x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y,0 });
        break;
    case 8:
        v.insert(v.end(), { x,y,0, x,y + 2 * s,0, x,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x + s,y,0, x + s,y,0, x,y,0, x,y + s,0, x + s,y + s,0 });
        break;
    case 9:
        v.insert(v.end(), { x + s,y,0, x + s,y + 2 * s,0, x + s,y + 2 * s,0, x,y + 2 * s,0, x,y + 2 * s,0, x,y + s,0, x,y + s,0, x + s,y + s,0 });
        break;
    }
}

unsigned int createTimerVAO(int hours, int mins, int secs) {
    std::vector<float> v;
    // POSICIÓN: Esquina inferior derecha
    // Ajusta estas coordenadas para mover el timer:
    float startX = 0.60f;   // Más positivo = más a la derecha
    float startY = -0.70f;  // Más negativo = más abajo
    float digitSize = 0.012f;
    float spacing = 0.035f;

    float x = startX;

    // HH:MM:SS
    createDigitVertices(v, hours / 10, x, startY, digitSize); x += spacing;
    createDigitVertices(v, hours % 10, x, startY, digitSize); x += spacing;

    // Dos puntos (:)
    v.insert(v.end(), { x + digitSize * 0.3f, startY + digitSize * 1.5f, 0, x + digitSize * 0.3f, startY + digitSize * 1.5f, 0 });
    v.insert(v.end(), { x + digitSize * 0.3f, startY + digitSize * 0.5f, 0, x + digitSize * 0.3f, startY + digitSize * 0.5f, 0 });
    x += spacing * 0.7f;

    createDigitVertices(v, mins / 10, x, startY, digitSize); x += spacing;
    createDigitVertices(v, mins % 10, x, startY, digitSize); x += spacing;

    // Dos puntos (:)
    v.insert(v.end(), { x + digitSize * 0.3f, startY + digitSize * 1.5f, 0, x + digitSize * 0.3f, startY + digitSize * 1.5f, 0 });
    v.insert(v.end(), { x + digitSize * 0.3f, startY + digitSize * 0.5f, 0, x + digitSize * 0.3f, startY + digitSize * 0.5f, 0 });
    x += spacing * 0.7f;

    createDigitVertices(v, secs / 10, x, startY, digitSize); x += spacing;
    createDigitVertices(v, secs % 10, x, startY, digitSize);

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

unsigned int createBatteryVAO(float percent) {
    std::vector<float> v;
    // POSICIÓN: Esquina superior izquierda
    // Ajusta estas coordenadas para mover la batería:
    float x = -0.85f;  // Más negativo = más a la izquierda
    float y = 0.75f;   // Más positivo = más arriba
    float w = 0.08f;   // Ancho de la batería
    float h = 0.04f;   // Alto de la batería

    // Contorno de batería
    v.insert(v.end(), {
        x, y, 0, x + w, y, 0,
        x + w, y, 0, x + w, y - h, 0,
        x + w, y - h, 0, x, y - h, 0,
        x, y - h, 0, x, y, 0
        });

    // Punta de batería (derecha)
    float tipW = 0.01f;
    v.insert(v.end(), {
        x + w, y - h * 0.3f, 0, x + w + tipW, y - h * 0.3f, 0,
        x + w + tipW, y - h * 0.3f, 0, x + w + tipW, y - h * 0.7f, 0,
        x + w + tipW, y - h * 0.7f, 0, x + w, y - h * 0.7f, 0
        });

    // Relleno de batería según porcentaje
    float fillW = (w - 0.008f) * (percent / 100.0f);
    if (fillW > 0.001f) {
        v.insert(v.end(), {
            x + 0.004f, y - 0.004f, 0, x + 0.004f + fillW, y - 0.004f, 0,
            x + 0.004f + fillW, y - 0.004f, 0, x + 0.004f + fillW, y - h + 0.004f, 0,
            x + 0.004f + fillW, y - h + 0.004f, 0, x + 0.004f, y - h + 0.004f, 0,
            x + 0.004f, y - h + 0.004f, 0, x + 0.004f, y - 0.004f, 0
            });
    }

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    return VAO;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Drone Simulation", nullptr, nullptr);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;
    glEnable(GL_DEPTH_TEST);

    Shader lightingShader("shaders/lighting.vs", "shaders/lighting.fs");

    Model house("C:/Users/angul/source/repos/OpenGl/OpenGl/model/scene2/Scnecp.obj");
    Model clouds("C:/Users/angul/source/repos/OpenGl/OpenGl/model/scene2/Clouds.obj");
    Model lightsModel("C:/Users/angul/source/repos/OpenGl/OpenGl/model/scene2/Lights.obj");

    ExtractData(house, lightsModel);

    // Shaders HUD
    const char* hudVS = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        void main() { 
            gl_Position = vec4(aPos, 1.0);
            TexCoords = aTexCoords;
        }
    )";
    const char* hudFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoords;
        uniform bool isWarning;
        uniform bool isText;
        uniform bool isFrame;
        uniform bool isTimer;
        uniform bool isBattery;
        uniform float time;
        uniform sampler2D frameTexture;
        void main() {
            if(isFrame) {
                FragColor = texture(frameTexture, TexCoords);
            } else if(isText) {
                float flash = sin(time * 6.0) * 0.3 + 0.7;
                FragColor = vec4(1.0, 1.0, 1.0, flash);
            } else if(isWarning) {
                float flash = sin(time * 8.0) * 0.3 + 0.5;
                FragColor = vec4(0.8, 0.0, 0.0, flash * 0.7);
            } else if(isTimer) {
                FragColor = vec4(1.0, 0.2, 0.2, 0.9);
            } else if(isBattery) {
                FragColor = vec4(0.2, 1.0, 0.2, 0.9);
            }
        }
    )";

    unsigned int vShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vShader, 1, &hudVS, NULL);
    glCompileShader(vShader);
    unsigned int fShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fShader, 1, &hudFS, NULL);
    glCompileShader(fShader);
    unsigned int hudProgram = glCreateProgram();
    glAttachShader(hudProgram, vShader);
    glAttachShader(hudProgram, fShader);
    glLinkProgram(hudProgram);
    glDeleteShader(vShader);
    glDeleteShader(fShader);

    unsigned int frameQuadVAO = setupQuadVAO();
    unsigned int warningVAO = setupWarningVAO();
    unsigned int textVAO = setupTextVAO();

    // Cambia esta ruta a tu imagen PNG
    unsigned int frameTexture = loadTexture("C:/Users/angul/source/repos/OpenGl/OpenGl/textures/marco.png");

    // Ubicaciones de uniforms
    std::vector<GLint> lightPosLocs(MAX_LIGHTS), lightColLocs(MAX_LIGHTS), lightIntLocs(MAX_LIGHTS);
    lightingShader.use();
    for (int i = 0; i < MAX_LIGHTS; i++) {
        std::string n = "pointLights[" + std::to_string(i) + "].";
        lightPosLocs[i] = glGetUniformLocation(lightingShader.ID, (n + "position").c_str());
        lightColLocs[i] = glGetUniformLocation(lightingShader.ID, (n + "color").c_str());
        lightIntLocs[i] = glGetUniformLocation(lightingShader.ID, (n + "intensity").c_str());
    }

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / SCR_HEIGHT, 0.1f, 500.0f);
    glm::vec3 lightColor(1.0f, 0.9f, 0.7f);

    unsigned int timerVAO = 0;
    unsigned int batteryVAO = 0;
    int lastSecond = -1;
    float lastBatteryUpdate = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Inicializar tiempo de inicio
        if (drone.startTime == 0.0f) {
            drone.startTime = currentFrame;
        }

        // Actualizar batería (pierde 1% cada 6 segundos = 100% dura 10 minutos)
        if (currentFrame - lastBatteryUpdate > 6.0f) {
            drone.batteryPercent = std::max(0.0f, drone.batteryPercent - 1.0f);
            lastBatteryUpdate = currentFrame;
        }

        // Actualizar timer VAO cada segundo
        int elapsedTime = (int)(currentFrame - drone.startTime);
        int currentSecond = elapsedTime % 60;
        if (currentSecond != lastSecond) {
            if (timerVAO != 0) glDeleteVertexArrays(1, &timerVAO);

            int hours = elapsedTime / 3600;
            int mins = (elapsedTime % 3600) / 60;
            int secs = elapsedTime % 60;
            timerVAO = createTimerVAO(hours, mins, secs);
            lastSecond = currentSecond;
        }

        // Actualizar batería VAO cuando cambia
        static float lastBatteryPercent = 100.0f;
        if (drone.batteryPercent != lastBatteryPercent) {
            if (batteryVAO != 0) glDeleteVertexArrays(1, &batteryVAO);
            batteryVAO = createBatteryVAO(drone.batteryPercent);
            lastBatteryPercent = drone.batteryPercent;
        }
        if (batteryVAO == 0) {
            batteryVAO = createBatteryVAO(drone.batteryPercent);
        }

        processInput(window);

        // Verificar distancia
        float dist = glm::length(camera.Position - SPAWN_POINT);
        if (dist > MAX_DISTANCE) {
            if (!drone.signalLost) {
                drone.signalLost = true;
                drone.signalLostTimer = currentFrame;
            }
            if (currentFrame - drone.signalLostTimer > RESPAWN_DELAY) {
                camera.Position = SPAWN_POINT;
                drone.velocity = glm::vec3(0.0f);
                drone.signalLost = false;
            }
        }
        else {
            drone.signalLost = false;
        }

        glClearColor(0.01f, 0.01f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Renderizado 3D
        lightingShader.use();
        lightingShader.setBool("thermalVision", drone.thermalVision);
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", camera.GetViewMatrix());
        lightingShader.setVec3("viewPos", camera.Position);

        int numActive = std::min((int)lampPositions.size(), MAX_LIGHTS);
        float lightIntensity = drone.lightsOn ? 35.0f : 0.0f;

        for (int i = 0; i < numActive; i++) {
            glUniform3fv(lightPosLocs[i], 1, glm::value_ptr(lampPositions[i]));
            glUniform3fv(lightColLocs[i], 1, glm::value_ptr(lightColor));
            glUniform1f(lightIntLocs[i], lightIntensity);
        }
        glUniform1i(glGetUniformLocation(lightingShader.ID, "numLights"), numActive);

        glm::mat4 model = glm::mat4(1.0f);
        lightingShader.setMat4("model", model);
        house.Draw(lightingShader);
        lightsModel.Draw(lightingShader);

        model = glm::rotate(glm::mat4(1.0f), currentFrame * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
        lightingShader.setMat4("model", model);
        clouds.Draw(lightingShader);

        // HUD
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(hudProgram);
        GLint locFrame = glGetUniformLocation(hudProgram, "isFrame");
        GLint locWarning = glGetUniformLocation(hudProgram, "isWarning");
        GLint locText = glGetUniformLocation(hudProgram, "isText");
        GLint locTime = glGetUniformLocation(hudProgram, "time");
        GLint locTimer = glGetUniformLocation(hudProgram, "isTimer");
        GLint locBattery = glGetUniformLocation(hudProgram, "isBattery");

        // Marco
        glUniform1i(locFrame, true);
        glUniform1i(locWarning, false);
        glUniform1i(locText, false);
        glUniform1i(locTimer, false);
        glUniform1i(locBattery, false);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, frameTexture);
        glUniform1i(glGetUniformLocation(hudProgram, "frameTexture"), 0);
        glBindVertexArray(frameQuadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Timer (esquina inferior derecha)
        if (timerVAO != 0) {
            glUniform1i(locFrame, false);
            glUniform1i(locTimer, true);
            glBindVertexArray(timerVAO);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, 200);
        }

        // Batería (esquina superior izquierda)
        if (batteryVAO != 0) {
            glUniform1i(locTimer, false);
            glUniform1i(locBattery, true);
            glBindVertexArray(batteryVAO);
            glLineWidth(2.5f);
            glDrawArrays(GL_LINES, 0, 100);
        }

        // Advertencia
        if (drone.signalLost) {
            glUniform1i(locBattery, false);
            glUniform1f(locTime, currentFrame);

            glUniform1i(locWarning, true);
            glBindVertexArray(warningVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            glUniform1i(locWarning, false);
            glUniform1i(locText, true);
            glBindVertexArray(textVAO);
            glLineWidth(2.5f);
            glDrawArrays(GL_LINES, 0, 100);
        }

        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}